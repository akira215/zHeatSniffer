# zSpyAHD

This repo is the source firmware developped only to listen to UART an print the received Raw message on the terminal

## Caveats


## AHD20 messages

AHD20 messages seems to be +12V level, serial at 9600 baud 8N1.
The received frame looks like (hex)

```
3a	30	33	30	33	30	30	30	31	30	30	30	33	46	36	0d	0a						
3a	30	33	30	33	30	36	30	30	30	33	30	30	41	30	30	30	30	32	34	46	0d	0a
3a	30	33	30	33	30	30	41	30	30	30	30	32	35	38	0d	0a						
3a	30	33	30	33	30	34	30	30	37	44	30	39	42	41	42	36	0d	0a				
3a	30	33	30	33	30	30	30	31	30	30	30	33	46	36	0d	0a						
3a	30	33	30	33	30	36	30	30	31	30	30	30	43	38	30	30	31	45	46	45	0d	0a
```

This data are like MODBUS ASCII, considering

 * `3a` in ASCII is `:`, which is the typical start byte
 * Stops byte are `0d` `0a` represented by `CR` `LF` in ASCII

So data are first to be converted in ASCII:

```
    :   03  03  00  01  00  03  F6  \CR \LF
    :   03  03  06	00	03	00	A0	00	02	4F  \CR \LF
    :   03  03  00	A0	00	02	58  \CR \LF
    :   03  03  04	00	7D	09	BA	B6  \CR \LF
    :   03  03  00	01	00	03	F6  \CR \LF
    :   03  03  06	00	10	00	C8	00	1E	FE  \CR \LF   
```

Last Byte is LRC checksum (to be performed on the ASCII converted frame):
 * Sum all the bytes except start `0x03 + 0x03 + 0x00 + 0x01 + 0x00 + 0x03` = `0x0a`
 * Two's complement the byte `0x100 - 0x0a` = `0xF6`


Frame could then be decoded using Modbus standard format:

### Query message - Function code 0x03

| Start | Slave address | Function code | Data (Starting address) | Data (Number of registers) | Error check | End |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| Start | XX | 03 | Hi \| Lo | Hi \| Lo | XX | Stop |

---

### Response message - Function code 0x03

| Start | Slave address | Function code | Data (Number of bytes) | Data (First register) | Data (Second register) | Data (Third register) | Error check | End |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| Start | XX | 03 | Number of bytes | Hi \| Lo | Hi \| Lo | Hi \| Lo | XX | Stop |


### Query message - Function code 0x10

| Start | Slave address | Function code | Data (Starting address) | Data (Number of registers) | Data (Number of bytes) | Data (Preset data 1st register) | Data (Preset data 2nd register) | Error check | End |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| Start | XX | 10 | Hi \| Lo | Hi \| Lo | Number of bytes | Hi \| Lo | Hi \| Lo | XX | Stop |

---

### Response message - Function code 0x10

| Start | Slave address | Function code | Data (Starting address) | Data (Number of registers) | Error check | End |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| Start | XX | 10 | Hi \| Lo | Hi \| Lo | XX | Stop |


When the master asking the slave (ID:0x03) on register `0x0001` for `0x0003` registers, slave answer by a pointer to the next register master should ask for:
 * Slave first answer `0x0003`(next function)`0x00a0` (next start register) and `0x0002` (number of register)
 * Master ask then from register `0x00a0` for `0x0002` registers
 * After Slave answering this query, Master ask again register `0x0001` for `0x0003` registers
 * Slave answer `0x0010`(next function) `0x00c8`(next start register) and `0x001e`(number of register)
 * Master then send the function `0x0010` (write register), starting at address `0x00c8` and for `0x001e` registers

### Current decoding address

| Address | Description | Comments |
| :---: | :---: | :---: |
| 0xA0  | Unknown | - |
| 0xA1  | Ambiant T° | °C x 100 |
| 0xCB  | Target T° ?? | °C x 100 |
| 0xD1  | Target T° ?? | °C x 100 |
| 0xDC  | Counter | Incremental at each write cmd |


### To be developped




## Usage and Examples





