# git.owenmorgan.com: the source and release backup

A read-only copy of the family repos and their releases on Owen's own server
(Owen, 2026-11-22), in case GitHub goes away. No accounts, no sign-up; not
advertised. Paths match GitHub's: `git.owenmorgan.com/telltaleatheist/<repo>`.

## How it stays in sync

Nothing in a release's steps changes: publish to GitHub as before.
`forgejo-sync.py` (on the server as `/usr/local/bin/forgejo-sync`, run by
`forgejo-sync.timer` every 10 minutes) copies from GitHub:

- every branch and tag of each repo in `REPOS`, pushed with `--prune`;
- every release's name and notes, under the same tag;
- the files of the newest 3 releases of each game (by tag prefix: `v`,
  `pm-v`, `mw1-v`, `mw1r-v`, `chi-v`...), each checked against GitHub's
  sha256; older releases keep their notes here, their files stay on GitHub.

A release made only on the server is left alone. To copy at once after a
publish: `ssh triton sudo systemctl start forgejo-sync`. Its log:
`journalctl -u forgejo-sync`.

The launcher reads releases from both homes and the newest version on either
wins; it downloads from GitHub first and the server second (the server's
bandwidth is metered). docs/launcher.md.

## The server

- Forgejo (a single binary, `/usr/local/bin/forgejo`) as `forgejo.service`,
  user `git`, data in `/var/lib/forgejo`, SQLite, on 127.0.0.1:3030;
  `/etc/forgejo/app.ini` with registration, OpenID, OAuth2, mail, actions,
  packages, federation, uploads and its own mirroring off, `LANDING_PAGE =
  explore`.
- HestiaCP serves `git.owenmorgan.com` with a proxy template `forgejo`
  (the headscale template with port 3030 and `client_max_body_size 2g`) and a
  Let's Encrypt certificate; the DNS record is Hestia's.
- The owner account `telltaleatheist` (admin) and the sync's token are in
  `/root/forgejo-admin.txt` and `/root/forgejo-sync.token` (root only). The
  token reaches git and curl through the environment and stdin, never their
  arguments.
- Updating Forgejo: replace the binary with a newer release from
  codeberg.org/forgejo/forgejo (check its sha256), `systemctl restart forgejo`.

## If GitHub goes away

Publish releases straight to the server (Forgejo's release API, the same
shape as GitHub's), stop the timer, and open the site to the public.
