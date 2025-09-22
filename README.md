# cmod-xmplay

**Browse and play modules from Modland directly inside XMPlay.**

No web browser. No manual downloads. Just search → play.

---

## ✨ What you get

* **Instant search** across Modland’s catalog (artist / song / all fields)
* **One-click add** or **ALT+double-click to play now**
* **Batch add** all visible results
* **Update checker** that compares your local DB with Modland and rebuilds when new files appear
* Optional **OpenMPT support** for a much wider set of classic/ancient formats
* **Preformatted titles** via an on-the-fly playlist (PLS) so the playlist shows `Artist – Song`
* Lightweight: one DLL + one SQLite DB

---

## ✅ Requirements

* **Windows** (same as XMPlay)
* **XMPlay 4.x or newer** → [https://www.un4seen.com/](https://www.un4seen.com/)
* **Internet connection** (to fetch and stream files)
* *(Optional)* **xmp-openmpt** plugin for extended/ancient formats → [https://lib.openmpt.org/](https://lib.openmpt.org/)

---

## 🚀 Installation (2 minutes)

1. Download from **Releases**: [https://github.com/jllodra/cmod-xmplay/releases](https://github.com/jllodra/cmod-xmplay/releases)

   * `xmp-cmod.dll` (required)
   * `cmod.db` (optional, saves time on first run)

2. Copy the file(s) into your XMPlay folder, e.g.
   `C:\Programs\xmplay40`
   *(You can use any subfolder too.)*

3. Launch **XMPlay**.

4. Assign a shortcut to open the plugin dialog (e.g. press **W**):

![image](https://github.com/user-attachments/assets/2872595e-9467-490b-868a-925fc740610c)

5. Close Options and press your hotkey (**W** in the example).

---

## 🧭 Usage

<img width="767" height="585" alt="image" src="https://github.com/user-attachments/assets/fef66f02-67e4-4091-a1ee-11ecf95bf1ec" />

* **Type ≥ 3 characters** to search (Artist / Song / All).
* **Double-click** a result → add to playlist.
* **ALT + double-click** → open & play immediately.
* **Right-click** → context menu (copy URL, search on ModArchive, etc).
* **Add all to playlist** → adds the current result set.
* **Check for updates in Modland…** → compares your DB date with `allmods.zip` and offers:

  * **Rebuild (default formats)**: XMPlay-native formats only.
  * **Rebuild (all formats)**: XMPlay + OpenMPT formats.

> The window title shows a small **DB badge** with the build date and whether it was built with default or all formats.

---

## 🗂️ Formats

<img width="371" height="308" alt="image" src="https://github.com/user-attachments/assets/d1992034-dd18-4fc3-9f50-e98c11cdb00d" />

* **Default (XMPlay only):** `mod, s3m, xm, it, mo3, mtm, umx`
* **All formats (with xmp-openmpt installed):**
  `mptm, mod, s3m, xm, it, 669, amf, ams, c67, dbm, digi, dmf, dsm, dsym, dtm, far, fmt, imf, ice, j2b, m15, mdl, med, mms, mt2, mtm, nst, okt, plm, psm, pt36, ptm, sfx, sfx2, st26, stk, stm, stx, stp, symmod, ult, wow, gdm, mo3, oxm, umx, xpk, ppm, mmcmp`

Choose **Check for updates → Rebuild (all formats)** to populate your DB with the extended list.

---

## ⚙️ Configuration (optional)

Create a `cmod.ini` **next to the plugin DLL**, do this only if you do not want to use a PLS file for some reason:

```ini
[cmod]
; Use on-the-fly PLS to control playlist titles.
; 1 = use PLS (recommended and default), 0 = add URLs directly (legacy behavior)
use_pls=1
```

## 🎨 Making the titles look better (optional)

### Stop XMPlay from overwriting titles after reading tags

By default, XMPlay can **replace** playlist titles once it reads file/module metadata (local files are pre-read; internet files only when played). To keep the PLS titles intact:

1. Open **Options → Titles** in XMPlay.
2. Set **Track title formatting** to **empty** (blank).
   *This prevents XMPlay from replacing the playlist’s PLS titles after tags are read.*
3. In **Options → Display → Columns** (right click on the playlist), set the **Track** column to **Title** (not “Full path”).

<img width="517" height="383" alt="image" src="https://github.com/user-attachments/assets/d1e68c7d-4f6b-4c26-966d-ad8c9619c59d" />

<img width="596" height="392" alt="image" src="https://github.com/user-attachments/assets/1180a206-9a20-43c3-a6ed-afad3c18ece9" />

<img width="517" height="379" alt="image" src="https://github.com/user-attachments/assets/53df3f8f-8868-4789-a048-b9fac7d295ad" />

> **Library note:** The library doesn’t use formatted playlist titles, so disabling title updates here won’t affect your library contents.

### If you still see some “module name” replacing your titles

That can happen if XMPlay had cached metadata from earlier plays. Exporting the library, then deleting the `xmplay.library` file and reimporting the library refreshed things in testing.

## ⚡ Enable caching (optional)

If you don’t want to re-download a song each time you play it:

![image](https://github.com/user-attachments/assets/05886950-225c-437b-9c91-1cb866f0e7c5)

Cached files storage path info: [https://www.un4seen.com/forum/?topic=20680.0](https://www.un4seen.com/forum/?topic=20680.0)

---

## 🔧 Tips & troubleshooting

* **No results?**
  Make sure you typed at least 3 characters. Try **Check for updates… → Rebuild** if your DB is old or missing.

* **Exact phrases:**
  Wrap your query in quotes: `"star control"`.

* **Hyphens / punctuation:**
  The search is tolerant to `-`, `()`, etc. If something doesn’t match as expected, try removing punctuation or using quotes.

* **Network issues / proxies:**
  If XMPlay can’t reach Modland, check your proxy/firewall. The update checker does an HTTP **HEAD** on `allmods.zip` to compare timestamps, then downloads only when needed.

* **Uninstall:**
  Close XMPlay and delete `xmp-cmod.dll` (and `cmod.db` if you want).

---

## 🙏 Credits

* **Modland** for hosting the largest public archive of module music.
* **XMPlay** by un4seen.
* **OpenMPT** for extended/ancient format support.

---

## 🎱

**I consider this plug-in as "feature-complete". Bugfixes and maintenance operations will be done when necessary.**

**If you find a bug or want a new feature, please open an issue.**

---

Enjoy, and spread the word 🎶

by **herotyc**
