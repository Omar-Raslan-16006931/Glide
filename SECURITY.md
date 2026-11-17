# Security

Please report security problems **privately** through GitHub's
[private vulnerability reporting](https://github.com/DosukaSOL/PushFPS/security/advisories/new), not as a public issue.

Useful to include: the PushFPS version (Settings → About), what an attacker could do, and the steps to reproduce.

What PushFPS does to keep you safe:
- Every release is built by GitHub Actions from this repository, and `SHA256SUMS.txt` lists the hash of every file.
  Check your download against it.
- Stream keys and streaming sign-ins are encrypted for your Windows user (DPAPI) and never written to logs.
- PushFPS never downloads or runs code from other sites. The installer only fetches Veyra's runtime package from
  Veyra's GitHub release and checks it against SHA-256 values pinned at build time.
- ANR files are never shipped or downloaded by PushFPS. When you install a download yourself, its archive is checked
  first: safe file names and types only, size limits, and the previous version is kept for rollback.

Windows Defender may flag a new, unsigned PushFPS release as `Trojan:Win32/Tecabans.STV!cl`. This is a false
positive from its cloud check on new files. It is reported to Microsoft each time.
