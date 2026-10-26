# Lab 2-2

- Typ: Laboratory
- Zadáno: 04.11.2025, 12:30
- Termín: 04.11.2025, 14:45
- Max. bodů: 4.00

---

### Při startu počítače vybírejte v menu **OSY Lab**.

#### Používejte Makefile.

#### Minicraft Server pokračování

**Primárně je potřeba mít server upravený, aby obsluhoval klienty přes vlákna (viz domácí příprava).**

- Server musí umět rozesílat MSG zprávy a polohu všech hráčů ostatním hráčům.
- Samotný server také umí rozesílat zprávy připojeným hráčům.
- Dále upravte generování herní mapy `game_map[y][x]` na velikost 20×20, kde jednotlivé pozice mohou nabývat hodnot 1–8.

**Minicraft Klient**

- Upravte klienta tak, abyste se zbavili funkce `poll` – rozdělte kód do následujících vláken:

  - Čtení ze socketu: zobrazuje zprávy od hráčů a ze serveru, aktualizuje polohu hráčů.
  - Čtení ze stdin: načítá vstup z klávesnice a řeší různé příkazy.
  - Vlákno generující náhodný pohyb v případě, že hráč napíše příkaz `START`; příkaz `STOP` tuto funkci vypíná.

- Můžete opět použít novou verzi připravené dynamické knihovny [libminicraftmap.so](assets/lab_2-2/libminicraftmap.so), která umí vykreslovat herní mapu o velikosti 20×20, aktuální polohu ostatních hráčů a typy bloků kolem hráče, případně bloky, které již lokální hráč objevil.

  - Deklarace funkcí viz [minicraft_map.h](assets/lab_2-2/minicraft_map.h)

    - `void map_init(void)` → příprava mapy
    - `void draw_map(void)` → samotné vykreslování mapy
    - `void update_map_from_area(int px, int py, const char *area_str)` → aktualizace údajů ze serveru do mapy, funkce očekává řetězec ve formátu: `"0,0,0,4,1,1,1,2,3"`
    - **nově: `void update_other_player(const char *name, int x, int y)` → aktualizace o poloze ostatních hráčů ze serveru do mapy: "jmeno", x, y**

- Zjednodušte zadávání příkazů pro pohyb na jednotlivé znaky, např.:

```
case 'w': case 'W': cmd = "MOVE UP\n"; break;
case 's': case 'S': cmd = "MOVE DOWN\n"; break;
case 'a': case 'A': cmd = "MOVE LEFT\n"; break;
case 'd': case 'D': cmd = "MOVE RIGHT\n"; break;
```

Linking `minicraft_client_threads` with shared library:

```
g++ -pthread minicraft_client_threads.o lib/libminicraftmap.so -Llib -lminicraftmap -lrt -o minicraft_client_threads
```

- Volitelný **BONUS**: Ochrana proti kolizím – dva hráči nemohou stát na stejném bloku.
