# cmod-xmplay

xmplay plugin to browse and play directly from modland.com (the biggest source of module music online).

## Installation

* Go to the Releases page https://github.com/jllodra/cmod-xmplay/releases
* Download `xmp-cmod.dll` and (optionally) `cmod.db`
* Copy the file(s) to the xmplay folder, for example: `C:\programs\xmplay40` (you can put the files in any subfolder you like, too)
* Launch xmplay (only tested in xmplay 4 or later): https://www.un4seen.com/
* Enable the plug-in in "DSP -> Plug-ins"

![image](https://github.com/user-attachments/assets/6174fa84-719a-4916-a9fd-9c9f354f47b2)

* Add a shortcut to open the plugin dialog:

![image](https://github.com/user-attachments/assets/2872595e-9467-490b-868a-925fc740610c)

And press a key, I have it binded to the `W` key.

* Close `Options` and press `W`.

## Usage

![image](https://github.com/user-attachments/assets/fc90521f-7c1b-4e5c-8e35-e9a5d28f632d)

* Double click on a song: Adds it to playlist
* Right click on a song: Opens context dialog with options
* ALT + Double click on a song: Opens the song
* "Add all to playlist" button adds all the results
* "Rebuild db" rebuilds the modland database by parsing `allmods.zip` in http://modland.com/

## Enable cache (optionally)

If you don't want to download a song every time you play it.

![image](https://github.com/user-attachments/assets/05886950-225c-437b-9c91-1cb866f0e7c5)

Cached files storage path: [https://www.un4seen.com/forum/?topic=20680.0](https://www.un4seen.com/forum/?topic=20680.0)

## Formats imported

With "Rebuild db" the xmplay supported formarts are parsed: `"mod","s3m","xm","it","mo3","mtm","umx"`.

If you add the [xmp-openmpt](https://lib.openmpt.org/libopenmpt/download/) plugin, you can play many other formants including ancient ones: 
"mptm","mod","s3m","xm","it","669","amf","ams","c67","dbm","digi","dmf","dsm","dsym","dtm","far","fmt","imf","ice","j2b","m15","mdl","med","mms","mt2","mtm","nst","okt","plm","psm","pt36","ptm","sfx","sfx2","st26","stk","stm","stx","stp","symmod","ult","wow","gdm","mo3","oxm","umx","xpk","ppm","mmcmp"

Use "Rebuild db (all formats)" to populate the database with all formats supported by both xmplay and xmp-openmpt.

---

Enjoy and spread the word,

herotyc
