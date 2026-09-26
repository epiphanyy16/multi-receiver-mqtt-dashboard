# NFC Integration Notes — Wearable Watch (ESW Project)

## 1. Which NFC tag to buy

**Use NTAG213 stickers/coins.** 144 bytes of memory is plenty for a URL. Confirm the listing explicitly says **"NTAG213"** and **"NDEF"** — cheap unbranded tags sometimes ship as Mifare Classic 1K, which iPhone doesn't read as smoothly and has licensing/format issues.

Form factor — pick based on enclosure design:

- **Flat adhesive sticker (~25mm round or small rectangle)** — thinnest, easiest to embed under a plastic surface. Search: `NTAG213 NFC sticker 25mm`.
- **Epoxy coin/puck (~2–3mm thick)** — more rugged, survives flexing and wear better, slightly bulkier. Search: `NTAG213 NFC coin tag epoxy`.

**Recommendation:** go with the epoxy coin/puck version for a wearable — it survives strap flexing and knocks better than a paper sticker.

## 2. Where to place it relative to the switch

- **Do not overlap the switch's bow mechanism.** The PTM216B needs both ends of the switch unobstructed to actuate — keep the tag fully outside that compression path, and don't let it bend/crush the antenna coil.
- **Avoid metal contact.** Any metal directly behind the tag (screws, metal housing, shielding) kills NFC range via eddy currents. Keep the tag on a plastic face. If metal contact is unavoidable, buy an **"anti-metal"/"on-metal"** variant (has a ferrite backing layer).
- **RF interference is not a real concern.** NFC (13.56 MHz) and BLE (2.4 GHz) are far enough apart in frequency that they won't interfere with each other. The real constraints are mechanical (don't crush the coil) and material (don't back it with metal).
- **Practical placement:** top face of the tray, off to one side of the switch, or on a raised plastic panel near the strap lugs — flat, outward-facing, so a phone can get parallel and within NFC's real range (~1–4cm).

## 3. Enclosure design notes

- Keep a **flat, unobstructed zone** for the tag (tag diameter + a couple mm margin) that is separate from the switch's moving/compressing path.
- Use a **thin plastic wall** between the tag and the outside — a few mm of plastic barely affects NFC range, but avoid thick/dense plastic or any metal behind it.
- Add a **rib/wall in the CAD** to physically separate the tag pocket from the switch compartment — this stops switch-actuation flex from reaching the tag, and lets you assemble/replace either part independently.
- **Recess the tag** slightly if using a sticker so edges don't peel during wear, or seal it fully under a thin printed cap.

## 4. How NFC works (quick reference)

- NFC operates at **13.56 MHz** using inductive coupling (like wireless charging, much weaker).
- The phone's reader generates an oscillating magnetic field; the tag's coil sits in that field and current is induced in it — this is what **powers the tag** (no battery needed).
- The tag sends data back via **load modulation**: it varies its current draw, which perturbs the phone's field slightly; the phone reads that perturbation as bits.
- Data is stored as **NDEF** (NFC Data Exchange Format) — typically a URI record pointing to your server with an ID, e.g. `https://yourserver.com/scan/<id>`.

## 5. Making the iPhone scan → open a login-gated details page

No custom app or Core NFC code needed — **Safari reads NDEF tags natively in the background since iOS 13+.**

**Flow:**

1. Write a single **NDEF URI record** to the tag: `https://yourserver.com/scan/<student_id>`. Do this once per tag using a free NFC-writing app (e.g. "NFC Tools" on iPhone or Android).
2. Tapping the tag with the phone unlocked shows a notification banner; tapping it opens the URL in Safari.
3. Server-side, that route checks for a login session/cookie:
   - **Not logged in** → redirect to your login page, then back to `/scan/<id>` after auth.
   - **Logged in** → look up `<id>` in your DB and render the student's details page.
4. **Nothing sensitive lives on the tag itself** — it only holds an ID + your domain. All gating and actual data happen server-side, so even if someone else taps it, they just hit your login wall.

**Summary of the loop:** tag → URL → auth check → DB lookup → details page.

## 6. For a Thursday deadline

- Skip native iOS/Core NFC entirely — it needs Xcode, a physical device, and buys you nothing over the URL approach for this use case.
- Optionally make the login page an **"Add to Home Screen"** PWA so it feels app-like (fullscreen, own icon) without actually being a native app.
- Minimum build: login page + session/cookie check + `/scan/<id>` route + DB lookup + details page. You likely already have most of the DB/backend pieces from the existing dashboard.
