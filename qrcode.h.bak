#define QR_CODE                 1
uchar	cal_qr								(void);
struct	QRcode *obtenQRcode					(void);
uchar	*obtenBufParser						(void);
uint	obtenNumDig						    (void);
//**************************************************************************************
// DEFINES                                                                             *
//**************************************************************************************

struct BitmapQR
{
	int size; 	/* 21 a 93 con saltos de 4 en 4 */
	unsigned char m[3360];
};

struct BitStringQR 
{
	unsigned int length; /* Numero de bits introducidos */
	unsigned char datos[3360];
};

struct QRcode
{
	unsigned char version;
	unsigned char error_level;
	struct BitmapQR codigo;
	struct BitmapQR colisiones;
	struct BitStringQR bs;
	unsigned int num_codewords;
	unsigned int num_error_codewords;
	unsigned int numero_bloques_1, numero_bloques_2;
	unsigned int num_datos_bloques_1, num_datos_bloques_2;
	unsigned int num_error_bloques_1, num_error_bloques_2;
	unsigned char error_level_code;
	unsigned char mascara;
	unsigned int format_info;
};


/* Numero de data codeword segun version y nivel de correccion de errores
   Tablas 7 a 11 del estandar */
static const int tbl_codewords_version[25][2] = {
    { 19,   16 },
    { 34,   28 },
    { 55,   44 },
    { 80,   64 },
    { 108,  86 },
    { 136,  108 },
    { 156,  124 },
    { 194,  154 },
    { 232,  182 },
    { 274,  216 },
    { 324,  254 },
    { 370,  290 },
    { 428,  334 },
    { 461,  365 },
    { 523,  415 },
    { 589,  453 },
    { 647,  507 },
    { 721,  563 },
    { 795,  627 },
    { 861,  669 },
    { 932,  714 },
    { 1006, 782 },
    { 1094, 860 },
    { 1174, 914 },
    { 1276, 1000 }
                                };
/* capacidades: numerico, alfanumerico, byte
   paginas 28 a 32 del documento */
static const uint tabla_capacidades_L[25][3] =
{
    { 41, 25, 17 },{ 77, 47, 32 },{ 127, 77, 53 },{ 187, 114, 78 },{ 255, 154, 106 },
    { 322, 195, 134 },{ 370, 224, 154 },{ 461, 279, 192 },{ 552, 335, 230 },{ 652, 395, 271 },
    { 772, 468, 321 },{ 883, 535, 367 },{ 1022, 619, 425 },{ 1101, 667, 458 },{ 1250, 758, 520 },
    { 1408, 854, 586 },{ 1548, 938, 644 },{ 1725, 1046, 718 },{ 1903, 1153, 792 },{ 2061, 1249, 858 },
    { 2232, 1352, 929 },{ 2409, 1460, 1003 },{ 2620, 1588, 1091 },{ 2812, 1704, 1171 },{ 3057, 1853, 1273 }
};

/* capacidades: numerico, alfanumerico, byte
   paginas 28 a 32 del documento */
static const uint tabla_capacidades_M[25][3] =
{
    {34, 20, 14}, {63, 38, 26}, {101, 61, 42}, {149, 90, 62}, {202, 122, 84},
    {255, 154, 106}, {293, 178, 122}, {365, 221, 152},  {432, 262, 180}, {513, 311, 213},
    {604, 366, 251}, {691, 419, 287}, {796, 483, 331}, {871, 528, 362}, {991, 600, 412},
    {1082, 656, 450}, {1212, 734, 504}, {1346, 816, 560}, {1500, 909, 624}, { 1600, 970, 666 },
    { 1708, 1035, 711 },{ 1872, 1134, 779 },{ 2059, 1248, 857 },{ 2188, 1326, 911 },{ 2395, 1451, 997 }
};

                           /* SP                  $    %                        *    +         -    .    /    0    1    2    3    4    5    6    7    8    9    :  */
static uchar tablaPermitidos[27] = { 1,   0,   0,   0,   1,   1,   0,   0,   0,   0,   1,   1,   0,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1 };
                        /* 0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3A*/


/* Numero total de error correction codewords segun version y nivel de correccion de errores
   Tablas 13 a 22 del estandar */
static const uint error_correction_codewords[25][2] =
{
    { 7,  10 },
    { 10, 16 },
    { 15, 26 },
    { 20, 36 },
    { 26, 48 },
    { 36, 64 },
    { 40, 72 },
    { 48, 88 },
    { 60, 110 },
    { 72, 130 },
    { 80, 150 },
    { 96, 176 },
    { 104,198 },
    { 120,216 },
    { 132,240 },
    { 144,280 },
    { 168,308 },
    { 180,338 },
    { 196,364 },
    { 224,416 },
    { 224,442 },
    { 252,476 },
    { 270,504 },
    { 300,560 },
    { 312,588 }
};

/* Caracteristicas de los bloques de error correction para nivel L
   Tablas 13 a 22 del estandar
   numero de bloques, bytes de datos, bytes de error correction, numero de bloques, bytes de datos, bytes de error correction */
static const uint error_correction_character_L[25][6] =
{
    { 1, 19, 7, 0, 0, 0 },
    { 1, 34, 10, 0, 0, 0 },
    { 1, 55, 15, 0, 0, 0 },
    { 1, 80, 20, 0, 0, 0 },
    { 1, 108, 26, 0, 0, 0 },
    { 2, 68, 18, 0, 0, 0 },
    { 2, 78, 20, 0, 0, 0 },
    { 2, 97, 24, 0, 0, 0 },
    { 2, 116, 30, 0, 0, 0 },
    { 2, 68, 18, 2, 69, 18 },
    { 4, 81, 20, 0, 0, 0 },
    { 2, 92, 24, 2, 93, 24 },
    { 4, 107, 26, 0, 0, 0 },
    { 3, 115, 30, 1, 116, 30 },
    { 5, 87, 22, 1, 88, 22 },
    { 5, 98, 24, 1, 99, 24 },
    { 1, 107, 28, 5, 108, 28 },
    { 5, 120, 30, 1, 121, 30 },
    { 3, 113, 28, 4, 114, 28 },
    { 3, 107, 28, 5, 108, 28 },
    { 4, 116, 28, 4, 117, 28 },
    { 2, 111, 28, 7, 112, 28 },
    { 4, 121, 30, 5, 122, 30 },
    { 6, 117, 30, 4, 118, 30 },
    { 8, 106, 26, 4, 107, 26 }
};


/* Caracteristicas de los bloques de error correction para nivel M
   Tablas 13 a 22 del estandar
   numero de bloques, bytes de datos, bytes de error correction, numero de bloques, bytes de datos, bytes de error correction */
static const uint error_correction_character_M [25][6] =
{
    { 1, 16, 10, 0, 0, 0 },
    { 1, 28, 16, 0, 0, 0 },
    { 1, 44, 26, 0, 0, 0 },
    { 2, 32, 18, 0, 0, 0 },
    { 2, 43, 24, 0, 0, 0 },
    { 4, 27, 16, 0, 0, 0 },
    { 4, 31, 18, 0, 0, 0 },
    { 2, 38, 22, 2, 39, 22 },
    { 3, 36, 22, 2, 37, 22 },
    { 4, 43, 26, 1, 44, 26 },
    { 1, 50, 30, 4, 51, 30 },
    { 6, 36, 22, 2, 37, 22 },
    { 8, 37, 22, 1, 38, 22 },
    { 4, 40, 24, 5, 41, 24 },
    { 5, 41, 24, 5, 42, 24 },
    { 7, 45, 28, 3, 46, 28 },
    { 10, 46, 28, 1, 47, 28 },
    { 9, 43, 26, 4, 44, 26 },
    { 3, 44, 26, 11, 45, 26 },
    { 3, 41, 26, 13, 42, 26 },
    { 17, 42, 26, 0, 0, 0 },
    { 17, 46, 28, 0, 0, 0 },
    { 4, 47, 28, 14, 48, 28 },
    { 6, 45, 28, 14, 46, 28 },
    { 8, 47, 28, 13, 48, 28 }
};

/* Tabla para calcular las coordenadas de los aligment patterns
  (Anexo E del estandar) */
static const uint coordenadas_alignment_patterns[25][8] =
{
    { 0, 0, 0, 0, 0, 0, 0, 0} ,
    { 2, 6, 18, 0, 0, 0, 0, 0},
    { 2, 6, 22, 0, 0, 0, 0, 0},
    { 2, 6, 26, 0, 0, 0, 0, 0},
    { 2, 6, 30, 0, 0, 0, 0, 0},
    { 2, 6, 34, 0, 0, 0, 0, 0},
    { 3, 6, 22, 38, 0, 0, 0, 0},
    { 3, 6, 24, 42, 0, 0, 0, 0},
    { 3, 6, 26, 46, 0, 0, 0, 0},
    { 3, 6, 28, 50, 0, 0, 0, 0},
    { 3, 6, 30, 54, 0, 0, 0, 0},
    { 3, 6, 32, 58, 0, 0, 0, 0},
    { 3, 6, 34, 62, 0, 0, 0, 0},
    { 4, 6, 26, 46, 66, 0, 0, 0},
    { 4, 6, 26, 48, 70, 0, 0, 0},
    { 4, 6, 26, 50, 74, 0, 0, 0},
    { 4, 6, 30, 54, 78, 0, 0, 0},
    { 4, 6, 30, 56, 82, 0, 0, 0},
    { 4, 6, 30, 58, 86, 0, 0, 0},
    { 4, 6, 34, 62, 90, 0, 0, 0 },
    { 5, 6, 28, 50, 72, 94, 0, 0 },
    { 5, 6, 26, 50, 74, 98, 0, 0 },
    { 5, 6, 30, 54, 78, 102, 0, 0 },
    { 5, 6, 28, 54, 80, 106, 0, 0 },
    { 5, 6, 32, 58, 84, 110, 0, 0 }
};

/* Informacion de la version (Pagina 79 del estandar) */
static bool version_information[34] = { 0x07C94, 0x085BC, 0x09A99, 0x0A4D3, 0x0BBF6, 0x0C762, 0x0D847,
                                         0x0E60D, 0x0F928, 0x10B78, 0x1145D, 0x12A17, 0x13532, 0x149A6,
                                         0x15683, 0x168C9, 0x177EC, 0x18EC4, 0x191E1, 0x1AFAB, 0x1B08E,
                                         0x1CC1A, 0x1D33F, 0x1ED75, 0x1F250, 0x209D5, 0x216F0, 0x228BA,
                                         0x2379F, 0x24B0B, 0x2542E, 0x26A64, 0x27541, 0x28C69 };
                                         
