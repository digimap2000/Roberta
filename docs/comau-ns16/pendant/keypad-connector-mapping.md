# Comau NS16 Pendant Keypad Connector Mapping


![[keypad-connectors.jpeg]]

| Working name | Physical identifier | Way count | Pin 1 orientation recorded | Notes |
| ------------ | ------------------- | --------- | -------------------------- | ----- |
| C1           | pending             | 9         | no                         |       |
| C2           | pending             | 9         | no                         |       |
| C3           | pending             | 9         | no                         |       |
| C4           | pending             | 9         | no                         |       |
| C5           | pending             | 9         | no                         |       |
| C6           | pending             | 9         | no                         |       |

## 1 - Axis 456
|        | Col 1 | Col 2 | Col 3 | Col 4 | Col 5 | Col 6 |   Col 7    |    Col 8    |
| :----: | :---: | :---: | :---: | :---: | :---: | :---: | :--------: | :---------: |
| Common |  6Z+  |  5Y+  |  4X+  |  4X-  |  6Z-  |  5Y-  | Aux B Plus | Aux B Minus |
NOT a matrix, just a straightforward 8 lines switched to a common pin. The common pin is the northernmost pin, then the columns ascend from 1 to 8 towards the bottom of the case.

## 2 - Axis 123
|        | Col 1 | Col 2 | Col 3 | Col 4 | Col 5 | Col 6 |   Col 7    |    Col 8    |
| :----: | :---: | :---: | :---: | :---: | :---: | :---: | :--------: | :---------: |
| Common |  3Z+  |  2Y+  |  1X+  |  1X-  |  3Z-  |  2Y-  | Aux A Plus | Aux A Minus |
NOT a matrix, just a straightforward 8 lines switched to a common pin. The common pin is the northernmost pin, then the columns ascend from 1 to 8 towards the bottom of the case.

## 3 - Number Pad
|     | Col 1 | Col 2  | Col 3 | Col 4 | Col 5 |
| :-: | :---: | :----: | :---: | :---: | :---: |
| R1  |       | Arrow  |       |       |       |
| R2  |   9   | Period |   6   |   3   |       |
| R3  |   8   |   0    |   5   |   2   |       |
| R4  |   7   | Minus  |   4   |   1   |       |
Column 5 appears to be completely unused on this connector.

## 4 - Jog Pad
|        | Col 1  | Col 2 | Col 3 | Col 4 | Col 5  | Col 6   | Col 7   | Col 8  |
| :----: | :----: | :---: | :---: | :---: | ------ | ------- | ------- | ------ |
| Common | J-DOWN | J-UP  |  +%   |  -%   | J-WEST | J-NORTH | J-SOUTH | J-EAST |
NOT a matrix, another 8:1 set of switches.

|       |  B1   | B2  |  B3   |  B4  |    B5     |   B6    |  B7   |  B8  |  B9   |  GND  |
| :---: | :---: | :-: | :---: | :--: | :-------: | :-----: | :---: | :--: | :---: | :---: |
|  T1   |  ESC  | F3  | ENTER |  L6  | FAST DOWN | FAST UP | COORD |  -   |   -   |   -   |
|  T2   | RMORE | R2  |  R3   |  L3  |    L2     |   L1    | DOWN  |  -   |   -   |   -   |
|  T3   |  R1   | R4  |  R5   |  L5  |    L4     |  LMORE  |  REC  |  -   |   -   |   -   |
|  T4   |  F5   | F6  |  R6   |  -   |   RESET   |  STEP   |   -   |  -   |   -   |   -   |
|  T5   |  F4   | F2  |  F1   | HELP |   LEFT    |   UP    | RIGHT |  -   |   -   |   -   |
|  T6   |   -   |  -  |   -   |  -   |     -     |    -    |   -   |  -   |   -   | START |
|  T7   |   -   |  -  |   -   |  -   |     -     |    -    |   -   |  -   |   -   | BACK  |
|  T8   |   -   |  -  |   -   |  -   |     -     |    -    |   -   |  -   | SHIFT |   -   |
| T9(?) |   -   |  -  |   -   |  -   |     -     |    -    |   -   |  -   |   -   |   -   |
|  GND  |   -   |  -  |   -   |  -   |     -     |    -    |   -   | HOLD |   -   |   -   |

START       T6  +  GND????
HOLD.       B8  +. GND????
BACK        T7  +  GND????

But why GND on the other connector and not GND on maybe T9?????