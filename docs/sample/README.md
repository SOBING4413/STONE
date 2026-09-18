# Contoh isi file JSON

File di folder ini adalah contoh nyata yang dihasilkan oleh kode STONE
(bukan tulisan tangan), lalu sebagian dipotong agar mudah dibaca:

| File | Catatan |
|---|---|
| `profile.json` | utuh |
| `settings.json` | utuh |
| `history.json` | utuh (3 catatan makanan + 2 catatan latihan) |
| `progress.json` | utuh |
| `foods.json` | **dipotong** — aplikasi sebenarnya berisi 40 makanan bawaan + makanan custom |
| `workouts.json` | **dipotong** — aplikasi sebenarnya berisi 12 program / 54 gerakan |
| `stone_backup.json` | **dipotong** — bundle backup berisi seluruh file di atas |

Saat aplikasi berjalan, file-file ini berada di
`/data/data/com.stone.app/files/`, dan file backup di
`/sdcard/Android/data/com.stone.app/files/stone_backup_YYYY-MM-DD.json`.

Enum yang dipakai di dalam JSON:

* `sex`: 0 = male, 1 = female
* `activity`: 0 sedentary, 1 light, 2 moderate, 3 active, 4 very active
* `goal`: 0 lose, 1 maintain, 2 gain
* `level`: 0 beginner, 1 intermediate, 2 advanced
* `category`: 0 full body, 1 upper body, 2 lower body, 3 core, 4 strength, 5 cardio
* `meal`: 0 breakfast, 1 lunch, 2 dinner, 3 snack
