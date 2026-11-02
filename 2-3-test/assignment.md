# Lab 2-3

- Typ: Laboratory
- Zadáno: 11.11.2025, 12:30
- Termín: 11.11.2025, 14:45
- Max. bodů: 4.00

---

**Při startu počítače vybírejte v menu OSY Lab.**

Používejte Makefile.

### Pizzerie

Vaším cílem je naprogramovat simulaci pizzerie, kde se připravují a odebírají pizzy. Výroba a spotřeba musí být synchronizována pomocí POSIX semaforů. Použijte pouze tři semafory (`mutex`, `empty`, `full`) a žádné další.

Pizzy se ukládají do fronty (skladu) s omezenou kapacitou. Pekaři (producenti) pizzy vyrábějí a vkládají do fronty, zákazníci (konzumenti) pizzy odebírají a potvrzují jejich přijetí.

#### Požadavky

##### 1. Funkce výroby a spotřeby

- Odstraňte z funkcí z domácí přípravy nekonečné smyčky while. Funkce přijímá položku jako argument:

  - `void producer(char *pizza)`
  - `void consumer(char *pizza_out)`
- Frontu lze implementovat libovolně (pole, kruhový buffer, vector …). Vkládání a vybírání hlídají pouze semafory; není třeba další synchronizace.

##### 2. Socketový server

- Server inicializuje frontu a semafory. Při připojení každého klienta vytvoří nové vlákno.

  - Klientovi pošle výzvu:

    - `Role?\n`
  - Klient odpoví:

    - `pekar` nebo `zakaznik`

- Server podle role vytvoří vlákno:

  - `pekar` → přijímá názvy pizz a ukládá je do fronty pomocí producer().
  - `zakaznik` → vybírá pizzy z fronty pomocí consumer(), vypisuje názvy a posílá klientovi potvrzení `OK\n`.

##### 3. Klient

- **A) Klient – pekař**

  - Po zvolení role `pekar` vytvoří nové vlákno. Posílá náhodně názvy pizz, např.:

    - Margherita, Prosciutto, Capricciosa, Quattro Formaggi, Diavola, atd.
  - Odesílá názvy na server po řádcích. Po každém odeslaném názvu čeká na potvrzení `OK\n`.
  - Počet je zadávaný ze stdin.

- **B) Klient – zákazník**

  - Po zvolení role zakaznik vytvoří nové vlákno. Přijímá názvy pizz ze serveru po řádcích.
  - Každý přijatý název vypíše na konzoli a odpoví `OK\n` serveru. Rychlost spotřeby není omezená.

##### 4. Paralelní běh

- Program musí podporovat současný běh více pekarů i zákazníků.
- Každý klient má vlastní vlákno na serveru.
- Paralelní komunikace se serverem a frontou musí být stabilní.

##### 5. Synchronizace

- Fronta má omezenou kapacitu:

  - Pekaři čekají, pokud je sklad plný.
  - Zákazníci čekají, pokud je sklad prázdný.
- Celou synchronizaci zajišťují pouze tři semafory: `mutex`, `empty`, `full`.

##### 6. Testovací scénář

- Spusťte server.
- Připojte jednoho nebo více pekařů, kteří začnou odesílat názvy pizz.
- Připojte jednoho nebo více zákazníků, kteří budou přijímat pizzy a potvrzovat `OK\n`.
- Sledujte:

  - Pekaři čekají při plném skladu
  - Zákazníci čekají při prázdném skladu
- Po dokončení testu lze odpojovat klienty a připojovat nové pekaře i zákazníky.
- Program musí běžet stabilně i při více současných klientech.
