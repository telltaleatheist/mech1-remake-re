#!/usr/bin/env python3
"""git.owenmorgan.com: copy the family repos and their releases from GitHub into Forgejo.

Read-only backup (Owen, 2026-11-22). Code: every branch and tag, pushed with --prune. Releases: every
GitHub release made on Forgejo under the same tag, its notes kept current; the files of the newest KEEP
releases of each game (tag prefix: v, pm-v, mw1-v, mw1r-v, chi-v...) copied and checked against GitHub's
sha256, older releases' files removed here (they stay on GitHub). A release only on Forgejo is left alone.
Run by forgejo-sync.timer; `sudo systemctl start forgejo-sync` runs it now.
"""
import hashlib, json, os, re, subprocess, sys, tempfile, urllib.error, urllib.parse, urllib.request

OWNER = "telltaleatheist"
REPOS = ["mw3-re", "mech3-re", "mech3-pm-re", "mech1-remake-re", "mech1-re",
         "inception-re", "inception-remake-re", "re-setup-kit"]
KEEP = 3
FORGEJO = "http://127.0.0.1:3030"
STATE = "/var/lib/forgejo-sync"
TOKEN = open("/root/forgejo-sync.token").read().strip()


def log(*a):
    print(*a, flush=True)


def req(url, method="GET", data=None, headers=None, token=False):
    h = {"User-Agent": "forgejo-sync", "Accept": "application/json"}
    if token:
        h["Authorization"] = "token " + TOKEN
    if data is not None:
        data = json.dumps(data).encode()
        h["Content-Type"] = "application/json"
    h.update(headers or {})
    r = urllib.request.Request(url, data=data, method=method, headers=h)
    try:
        with urllib.request.urlopen(r, timeout=60) as resp:
            body = resp.read()
            return resp.status, (json.loads(body) if body else None), resp.headers
    except urllib.error.HTTPError as e:
        text = e.read()[:300].decode("utf-8", "replace")
        return e.code, {"error": text}, e.headers


def fj(path, method="GET", data=None):
    return req(FORGEJO + "/api/v1" + path, method, data, token=True)


def gh_get(url, key, cache):
    """A GitHub API answer; an ETag keeps an unchanged one off the 60-an-hour limit."""
    hdr = {"Accept": "application/vnd.github+json"}
    if key in cache:
        hdr["If-None-Match"] = cache[key]["etag"]
    code, body, headers = req(url, headers=hdr)
    if code == 304:
        return cache[key]["body"]
    if code != 200:
        raise RuntimeError(f"GitHub {url}: HTTP {code}")
    cache[key] = {"etag": headers.get("ETag", ""), "body": body}
    return body


def gh_releases(repo, cache):
    """GitHub's releases, all pages; an ETag keeps unchanged lists off the rate limit."""
    out, page = [], 1
    while True:
        body = gh_get(f"https://api.github.com/repos/{OWNER}/{repo}/releases?per_page=100&page={page}",
                      f"{repo}:{page}", cache)
        out += body
        if len(body) < 100:
            return out
        page += 1


def git(*a, cwd=None, auth=False):
    env = dict(os.environ)
    if auth:  # in the environment, never the arguments: other users on the box can read those
        env.update({"GIT_CONFIG_COUNT": "1", "GIT_CONFIG_KEY_0": "http.http://127.0.0.1:3030/.extraHeader",
                    "GIT_CONFIG_VALUE_0": "Authorization: token " + TOKEN})
    subprocess.run(["git", *a], cwd=cwd, env=env, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)


def sync_code(repo, meta):
    code, info, _ = fj(f"/repos/{OWNER}/{repo}")
    if code == 404:
        code, info, _ = fj("/user/repos", "POST", {"name": repo, "private": False,
                                                    "description": meta.get("description") or ""})
        if code not in (200, 201):
            raise RuntimeError(f"cannot make {repo} on Forgejo: HTTP {code}")
        log(f"{repo}: made on Forgejo")
    bare = os.path.join(STATE, "git", repo + ".git")
    if not os.path.isdir(bare):
        git("init", "--bare", "-q", bare)
    git("fetch", "-q", "--prune", f"https://github.com/{OWNER}/{repo}.git",
        "+refs/heads/*:refs/heads/*", "+refs/tags/*:refs/tags/*", cwd=bare)
    heads = subprocess.run(["git", "for-each-ref", "refs/heads"], cwd=bare, capture_output=True, text=True).stdout
    if not heads.strip():
        return  # an empty repo: nothing to push yet
    git("push", "-q", "--prune", f"http://127.0.0.1:3030/{OWNER}/{repo}.git",
        "+refs/heads/*:refs/heads/*", "+refs/tags/*:refs/tags/*", cwd=bare, auth=True)
    want = {"description": meta.get("description") or "", "website": meta.get("homepage") or "",
            "default_branch": meta.get("default_branch") or "main"}
    if any((info or {}).get(k) != v for k, v in want.items()):
        fj(f"/repos/{OWNER}/{repo}", "PATCH", want)


def prefix(tag):
    return re.sub(r"v?\d[\d.]*.*$", "", tag)


def download(url, digest, path):
    h = hashlib.sha256()
    r = urllib.request.Request(url, headers={"User-Agent": "forgejo-sync", "Accept": "application/octet-stream"})
    with urllib.request.urlopen(r, timeout=300) as resp, open(path, "wb") as f:
        while chunk := resp.read(1 << 20):
            f.write(chunk)
            h.update(chunk)
    if digest and digest.startswith("sha256:") and digest[7:] != h.hexdigest():
        raise RuntimeError(f"{url}: sha256 does not match GitHub's")


def upload(repo, rid, name, path):
    # The token through stdin (-H @-), not the arguments.
    r = subprocess.run(["curl", "-sf", "-o", "/dev/null", "-w", "%{http_code}", "-X", "POST",
                        "-H", "@-", "-F", f"attachment=@{path};filename={name}",
                        f"{FORGEJO}/api/v1/repos/{OWNER}/{repo}/releases/{rid}/assets?name={urllib.parse.quote(name)}"],
                       input="Authorization: token " + TOKEN + "\n", capture_output=True, text=True)
    if r.stdout.strip() not in ("200", "201"):
        raise RuntimeError(f"upload {repo} {name}: HTTP {r.stdout.strip()} {r.stderr.strip()}")


def sync_releases(repo, gh, seen):
    fjl, page = [], 1
    while True:
        code, body, _ = fj(f"/repos/{OWNER}/{repo}/releases?limit=50&page={page}")
        if code != 200:
            raise RuntimeError(f"Forgejo releases of {repo}: HTTP {code}")
        fjl += body
        if len(body) < 50:
            break
        page += 1
    have = {r["tag_name"]: r for r in fjl}
    live = [r for r in gh if not r.get("draft")]
    live.sort(key=lambda r: r.get("published_at") or "", reverse=True)
    count = {}
    for g in live:
        tag = g["tag_name"]
        p = prefix(tag)
        count[p] = count.get(p, 0) + 1
        keep_files = count[p] <= KEEP
        want = {"tag_name": tag, "name": g.get("name") or tag, "body": g.get("body") or "",
                "prerelease": bool(g.get("prerelease")), "draft": False}
        f = have.get(tag)
        if not f:
            code, f, _ = fj(f"/repos/{OWNER}/{repo}/releases", "POST", want)
            if code not in (200, 201):  # a tag just pushed is taken in a moment later: the next run makes it
                log(f"{repo} {tag}: cannot make the release yet: HTTP {code} {(f or {}).get('error', '')}")
                continue
            log(f"{repo} {tag}: release made")
        elif any(f.get(k) != want[k] for k in ("name", "body", "prerelease")):
            fj(f"/repos/{OWNER}/{repo}/releases/{f['id']}", "PATCH", want)
        fa = {a["name"]: a for a in f.get("assets") or []}
        if not keep_files:
            for a in fa.values():
                fj(f"/repos/{OWNER}/{repo}/releases/{f['id']}/assets/{a['id']}", "DELETE")
                log(f"{repo} {tag}: {a['name']} removed here (older than the last {KEEP})")
            continue
        for a in g.get("assets") or []:
            key = f"{repo}/{tag}/{a['name']}"
            stamp = (a.get("digest") or "") + "|" + str(a.get("size")) + "|" + (a.get("updated_at") or "")
            old = fa.get(a["name"])
            if old and old.get("size") == a.get("size") and seen.get(key) in (None, stamp):
                seen[key] = stamp
                continue
            if old:
                fj(f"/repos/{OWNER}/{repo}/releases/{f['id']}/assets/{old['id']}", "DELETE")
            with tempfile.NamedTemporaryFile(dir=os.path.join(STATE, "tmp"), delete=True) as t:
                download(a["browser_download_url"], a.get("digest"), t.name)
                upload(repo, f["id"], a["name"], t.name)
            seen[key] = stamp
            log(f"{repo} {tag}: {a['name']} copied ({a.get('size', 0) // 1000000} MB)")


def main():
    os.makedirs(os.path.join(STATE, "git"), exist_ok=True)
    os.makedirs(os.path.join(STATE, "tmp"), exist_ok=True)
    state_file = os.path.join(STATE, "state.json")
    try:
        state = json.load(open(state_file))
    except (OSError, ValueError):
        state = {}
    cache, seen = state.setdefault("etags", {}), state.setdefault("assets", {})
    failed = False
    for repo in REPOS:
        try:
            meta = gh_get(f"https://api.github.com/repos/{OWNER}/{repo}", repo, cache)
            sync_code(repo, meta)
            sync_releases(repo, gh_releases(repo, cache), seen)
        except Exception as e:  # one repo's trouble never stops the others
            failed = True
            log(f"{repo}: {e}")
        tmp = json.dumps(state)
        with open(state_file + ".new", "w") as f:
            f.write(tmp)
        os.replace(state_file + ".new", state_file)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
