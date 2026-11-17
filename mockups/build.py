import base64, re, sys, os
os.chdir(os.path.dirname(os.path.abspath(__file__)))
t = open('mw1hub.template.html').read()
t = t.replace("position: absolute; right: 3%; top: 13.5cqw; width: 23%;", "position: absolute; right: 3%; bottom: 5.4cqw; width: 23%;")
t = t.replace("<h2>CRESCENT HAWKS</h2>", "<h2>BRAVER'S COMPANY</h2>")
t = t.replace("width: 23%;\n  display: grid; grid-template-columns: 1fr 1fr;", "width: 12.5%;\n  display: grid; grid-template-columns: 1fr;")
for k in ("contract", "crew", "comstar"):
    t = re.sub(r'\n    <button class="svc" data-svc="%s"[^\n]*' % k, '', t)
    t = re.sub(r'\n  %s: \[[^\n]*' % k, '', t)
uri = lambda f: "data:image/jpeg;base64," + base64.b64encode(open(f, 'rb').read()).decode()
subs = {"BG": "bg.jpg", "BAR": "bar.jpg", "P1": "p1.jpg", "P2": "p2.jpg", "P3": "p3.jpg"}
subs.update({"ICON_" + k: "icon_%s.jpg" % k for k in ("mech", "bar", "starport")})
for k, f in subs.items():
    t = t.replace("{{%s}}" % k, uri(f))
assert "{{" not in t, re.findall(r"\{\{\w+\}\}", t)
open('mw1-hub.html', 'w').write(t)
print(len(t))
