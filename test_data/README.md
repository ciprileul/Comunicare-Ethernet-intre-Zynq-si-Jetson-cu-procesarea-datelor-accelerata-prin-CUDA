# test_data/

Fisierele de intrare care se copiaza pe cardul SD al placii Zynq:
- `IMG.BMP`  - imagine BMP necomprimata, 24 biti (ex. 640x480);
- `TEXT.TXT` - fisier text cu litere mici si mari, pentru test.

Pune aici si imagini de dimensiuni diferite (ex. 320x240, 640x480, 1280x720)
pentru masuratorile de performanta.

Numele trebuie sa respecte formatul 8.3 (max 8 caractere + extensie de 3),
pentru ca in BSP optiunea `use_lfn` (nume lungi) este dezactivata.

`out/` - rezultatele salvate la rulare (ignorat de Git).
