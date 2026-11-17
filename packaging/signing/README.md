# Code signing: where Glide stands

Goal: Windows shows "Verified publisher" instead of "Unknown publisher", without putting the owner's
real name or address on the certificate.

## The options

| Route | Name on the certificate | Cost | Fits Glide? |
|---|---|---|---|
| **SignPath Foundation** (free for open source) | "SignPath Foundation" | Free | Probably not today, see below |
| Microsoft Artifact Signing (was Trusted Signing) | Your legal name | ~US$10 a month | Individuals only in the USA and Canada; shows your name |
| Certum "Open Source Developer" and other CA certificates | Your legal name | €25 to €500 a year | Shows your name |

Only SignPath Foundation keeps the owner's identity off the certificate, so it is the only route that
meets the privacy requirement.

## Why SignPath Foundation will probably say no (as of November 2026)

From https://signpath.org/terms:

1. **No proprietary code.** All components must be under an OSI-approved licence; only "System
   Libraries" may be included. `Glide.exe` links NVIDIA's NGX SDK static library (`nvsdk_ngx_s.lib`,
   proprietary), and the runtime package contains NVIDIA, AMD and Intel runtimes.
2. **Forks.** A modified upstream fork may be signed only if the upstream project publishes signed
   builds. Veyra's releases are not signed.
3. Also required: every team member uses MFA on GitHub and SignPath; a "Code signing policy" on the
   home and download pages (text below); manual approval of every signing request; fixed product name
   and version in the file metadata.

Signing would not remove SmartScreen warnings on day one either: even signed files start without
reputation, which builds up with downloads over weeks.

## What would change the answer

- Veyra starts signing its releases (ask Likely7; SignPath also accepts upstream projects), **and**
- the NVIDIA SDK question is settled (SignPath may treat the NGX library as acceptable or not; only
  they can say). Asking costs nothing: apply at https://signpath.org/apply.html and describe the
  NVIDIA/AMD/Intel SDK linking honestly.

## If you apply

Use your GitHub account (DosukaSOL) with MFA on, and the project's public URLs only. The form asks for a
contact email; a GitHub noreply address will not receive mail, so use an address you are comfortable
sharing with SignPath (it is not published).

Code signing policy to add to the website and README once approved (not before):

> **Code signing policy.** Free code signing provided by [SignPath.io](https://signpath.io),
> certificate by [SignPath Foundation](https://signpath.org).
> Committers and reviewers: [DosukaSOL](https://github.com/DosukaSOL). Approvers: DosukaSOL.
> Privacy: Glide sends nothing to other computers unless you ask it to (update check, Discord
> sharing, streaming sources, downloads you start). Details in PRIVACY.md.

CI side, once SignPath gives an organisation ID and project slug: the release job uploads the built
zip's `Glide.exe` and installer scripts to SignPath with `signpath/github-action-submit-signing-request`,
waits for manual approval, and packages the signed files. Secrets: `SIGNPATH_API_TOKEN`,
`SIGNPATH_ORGANIZATION_ID`.
