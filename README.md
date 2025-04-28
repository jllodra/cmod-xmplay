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

![image](https://github.com/user-attachments/assets/fcd6a24d-908b-4df0-9464-dc9df7ba19e5)

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

Currently only these formats (file extensions to be more precise) are parsed from modland: `"mod","s3m","xm","it","mo3","mtm","umx"`.

xmplay only supports these formats afaik. Adding the [xmp-openmpt](https://lib.openmpt.org/libopenmpt/download/) plugin it should be possible to play many other formats.

If there's interest in having all those ancient formats available, fill an issue and let me know.

---

Enjoy and spread the word,

herotyc
