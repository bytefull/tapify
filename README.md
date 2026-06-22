# Tapify firmware

## Using host file system with native_sim

```bash
/workdir/application$ rm -rf /tmp/zephyr_settings
/workdir/application$ mkdir -p /tmp/zephyr_settings
/workdir/application$ rm -rf build/
/workdir/application$ rm -rf flash.bin
/workdir/application$ west build -b native_sim -d build/
/workdir/application$ ./build/zephyr/zephyr.exe -volume=/tmp/zephyr_settings:/settings
```

## ✅ TODO

- [x] Make signature verification works
- [x] Add keys generation instructions to readme
- [x] Add web server to serve bootstrap minimal UI via gzip compressed assets
- [x] Make the project build configurable such us I can easily build web alone or auth alone
- [x] Enable Networking on native_sim (using offloaded sockets)
- [x] Install needed tools on the docker image
- [x] Add PurgeCSS to the build pipeline
- [x] Fix very slow page loading problem
- [ ] Fix enabling apps in CMakeLists.txt
- [ ] Export keys from their files without dumping them in code
- [ ] Enable TF-M on STM32 and don't break the build on native_sim
- [ ] Enable Networking on STM32 via USB CDC ECM
- [ ] Design provisioning sequence
- [ ] Bundle all web css and js into a single index.html file automatically

## Android app logs:

```log
2026-05-16 19:21:17.503 13581-13581 MyHostApduService       com.example.androidnfchce            I  ================ Started MyHostApduService ================
2026-05-16 19:21:17.503 13581-13581 MyHostApduService       com.example.androidnfchce            I  Private key loaded successfully
2026-05-16 19:21:17.509 13581-13581 MyHostApduService       com.example.androidnfchce            I  ================ APDU RECEIVED ================
2026-05-16 19:21:17.510 13581-13581 MyHostApduService       com.example.androidnfchce            D  APDU received: 00 A4 04 00 08 F1 23 45 67 89 AB CD E1 00
2026-05-16 19:21:17.510 13581-13581 MyHostApduService       com.example.androidnfchce            I  ================ SELECT AID ================
2026-05-16 19:21:18.557 13581-13581 MyHostApduService       com.example.androidnfchce            I  ================ APDU RECEIVED ================
2026-05-16 19:21:18.557 13581-13581 MyHostApduService       com.example.androidnfchce            D  APDU received: 80 10 00 00 20 3B DB 31 CC B6 D5 08 AA D2 E1 BE 87 E4 7B 88 AE B1 9D E6 88 9E FB 2D 97 50 16 F7 28 31 2F C0 E6
2026-05-16 19:21:18.557 13581-13581 MyHostApduService       com.example.androidnfchce            I  ================ CHALLENGE ================
2026-05-16 19:21:18.562 13581-13581 MyHostApduService       com.example.androidnfchce            D  DER signature: 30 46 02 21 00 C3 7C 73 90 58 DA 26 6C D7 29 EF 89 EF E6 24 BE E9 00 AF AC BD 00 53 1F 28 4C 0A 05 DD 4A 51 FA 02 21 00 85 38 70 29 ED A7 DB C1 A7 95 AF C7 F9 42 83 F6 DC CD 26 23 ED A1 B7 4E 32 25 22 A2 40 C3 B3 2E
2026-05-16 19:21:18.563 13581-13581 MyHostApduService       com.example.androidnfchce            D  RAW signature: C3 7C 73 90 58 DA 26 6C D7 29 EF 89 EF E6 24 BE E9 00 AF AC BD 00 53 1F 28 4C 0A 05 DD 4A 51 FA 85 38 70 29 ED A7 DB C1 A7 95 AF C7 F9 42 83 F6 DC CD 26 23 ED A1 B7 4E 32 25 22 A2 40 C3 B3 2E
2026-05-16 19:21:18.563 13581-13581 MyHostApduService       com.example.androidnfchce            I  ================ CHALLENGE SUCCESS ================
2026-05-16 19:21:20.532 13581-13581 MyHostApduService       com.example.androidnfchce            D  Deactivated: 0
```

## Zephyr app logs:

```log
user@14566f1646fe:/workdir/application$ ./build/zephyr/zephyr.exe
WARNING: Using a test - not safe - entropy source
uart connected to pseudotty: /dev/pts/2
uart_1 connected to the serial port: /dev/ttyUSB0
*** Booting Zephyr OS build v4.3.0 ***
[1970-01-01 00:00:00.020,000] <inf> main: System ready. Waiting for NFC target...
[1970-01-01 00:00:05.780,000] <inf> main: Target detected
[1970-01-01 00:00:06.880,000] <inf> main: Received SELECT AID response
[1970-01-01 00:00:06.880,000] <dbg> main: main: Response:
                                          90 00                                            |..               
[1970-01-01 00:00:06.880,000] <inf> main: AID selected
[1970-01-01 00:00:06.880,000] <dbg> main: main: Generated (32U)-byte nonce:
                                          3b db 31 cc b6 d5 08 aa  d2 e1 be 87 e4 7b 88 ae |;.1..... .....{..
                                          b1 9d e6 88 9e fb 2d 97  50 16 f7 28 31 2f c0 e6 |......-. P..(1/..
[1970-01-01 00:00:07.960,000] <inf> main: Received signature response
[1970-01-01 00:00:07.960,000] <dbg> main: main: Response:
                                          c3 7c 73 90 58 da 26 6c  d7 29 ef 89 ef e6 24 be |.|s.X.&l .)....$.
                                          e9 00 af ac bd 00 53 1f  28 4c 0a 05 dd 4a 51 fa |......S. (L...JQ.
                                          85 38 70 29 ed a7 db c1  a7 95 af c7 f9 42 83 f6 |.8p).... .....B..
                                          dc cd 26 23 ed a1 b7 4e  32 25 22 a2 40 c3 b3 2e |..&#...N 2%".@...
                                          90 00                                            |..               
[1970-01-01 00:00:07.960,000] <inf> main: Verifying signature...
[1970-01-01 00:00:07.960,000] <inf> main: AUTH SUCCESS (verified signature)
```
