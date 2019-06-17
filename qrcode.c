//**************************************************************************************
// QRCode     							08-Abr-19                                      *
// Tarea de QR.                                                                    *
//**************************************************************************************

#include <LPC22xx.h>					// LPC22xx definitions
#include <string.h>
#include <stdlib.h>
#include "defines.h"
#include ".\RTX\Monitor.h"
#include "TimeLibCtrl.h"
#include "parser.h"
#include "imagen.h"
#include "qrcode.h"
#include "Comms\ProtocoloDebug232.h"
#include "Assert.h"
  #if CORR_CAMBIO_FTO
#include "RegistrosImpresion.h"
#include ".\DrvHw\DrvFPGA.h"
  #endif
  
#if QR_CODE

/* Numero de data codeword segun version y nivel de correccion de errores
   Tablas 7 a 11 del estandar */
static const int tbl_codewords_version[40][2] = {
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
    { 1276, 1000 },
    { 1370, 1062 },
    { 1468, 1128 },
    { 1531, 1193 },
    { 1631, 1267 },
    { 1735, 1373 },
    { 1843, 1455 },
    { 1955, 1541 },
    { 2071, 1631 },
    { 2191, 1725 },
    { 2306, 1812 },
    { 2434, 1914 },
    { 2566, 1992 },
    { 2702, 2102 },
    { 2812, 2216 },
    { 2956, 2334 }
                                };
//capacidades: numerico, alfanumerico, byte
//paginas 28 a 32 del documento
static const unsigned int tabla_capacidades_L[40][3] =
{
    {41, 25, 17}, {77, 47, 32}, {127, 77, 53}, {187, 114, 78}, {255, 154, 106},
    {322, 195, 134}, {370, 224, 154}, {461, 279, 192}, {552, 335, 230}, {652, 395, 271},
    {772, 468, 321}, {883, 535, 367}, {1022, 619, 425}, {1101, 667, 458}, {1250, 758, 520},
    {1408, 854, 586}, {1548, 938, 644}, {1725, 1046, 718}, {1903, 1153, 792}, {2061, 1249, 858},
    {2232, 1352, 929}, {2409, 1460, 1003}, {2620, 1588, 1091}, {2812, 1704, 1171}, {3057, 1853, 1273},
    {3283, 1990, 1367}, {3517, 2132, 1465}, {3669, 2223, 1528}, {3909, 2369, 1628}, {4158, 2520, 1732},
    {4417, 2677, 1840}, {4686, 2840, 1952}, {4965, 3009, 2068}, {5253, 3183, 2188}, {5529, 3351, 2303},
    {5836, 3537, 2431}, {6153, 3729, 2563}, {6479, 3927, 2699}, {6743, 4087, 2809}, {7089, 4296, 2953}
};

//capacidades: numerico, alfanumerico, byte
//paginas 28 a 32 del documento
static const unsigned int tabla_capacidades_M[40][3] =
{
    {34, 20, 14}, {63, 38, 26}, {101, 61, 42}, {149, 90, 62}, {202, 122, 84},
    {255, 154, 106}, {293, 178, 122}, {365, 221, 152},  {432, 262, 180}, {513, 311, 213},
    {604, 366, 251}, {691, 419, 287}, {796, 483, 331}, {871, 528, 362}, {991, 600, 412},
    {1082, 656, 450}, {1212, 734, 504}, {1346, 816, 560}, {1500, 909, 624}, {1600, 970, 666},
    {1708, 1035, 711}, {1872, 1134, 779}, {2059, 1248, 857}, {2188, 1326, 911}, {2395, 1451, 997},
    {2544, 1542, 1059}, {2701, 1637, 1125}, {2857, 1732, 1190}, {3035, 1839, 1264}, {3289, 1994, 1370},
    {3486, 2113, 1452}, {3693, 2238, 1538}, {3909, 2369, 1628}, {4134, 2506, 1722}, {4343, 2632, 1809},
    {4588, 2780, 1911}, {4775, 2894, 1989}, {5039, 3054, 2099}, {5313, 3220, 2213}, {5596, 3391, 2331}
};

                           /* SP                  $    %                        *    +         -    .    /    0    1    2    3    4    5    6    7    8    9    :  */
static uchar tablaPermitidos[27] = { 1,   0,   0,   0,   1,   1,   0,   0,   0,   0,   1,   1,   0,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1 };
                        /* 0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3A*/


/* Numero total de error correction codewords segun version y nivel de correccion de errores
   Tablas 13 a 22 del estandar */
static const uint error_correction_codewords[40][2] =
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
    { 312,588 },
    { 336,644 },
    { 360,700 },
    { 390,728 },
    { 420,784 },
    { 450,812 },
    { 480,868 },
    { 510,924 },
    { 540,980 },
    { 570,1036 },
    { 570,1064 },
    { 600,1120 },
    { 630,1204 },
    { 660,1260 },
    { 720,1316 },
    { 750,1372 }
};

/* Caracteristicas de los bloques de error correction para nivel L
   Tablas 13 a 22 del estandar
   numero de bloques, bytes de datos, bytes de error correction, numero de bloques, bytes de datos, bytes de error correction */
static const unsigned int error_correction_character_L [40][6] =
{
    {1, 19, 7, 0, 0, 0},
    {1, 34, 10, 0, 0, 0},
    {1, 55, 15, 0, 0, 0},
    {1, 80, 20, 0, 0, 0},
    {1, 108, 26, 0, 0, 0},
    {2, 68, 18, 0, 0, 0},
    {2, 78, 20, 0, 0, 0},
    {2, 97, 24, 0, 0, 0},
    {2, 116, 30, 0, 0, 0},
    {2, 68, 18, 2, 69, 18},
    {4, 81, 20, 0, 0, 0},
    {2, 92, 24, 2, 93, 24},
    {4, 107, 26, 0, 0, 0},
    {3, 115, 30, 1, 116, 30},
    {5, 87, 22, 1, 88, 22},
    {5, 98, 24, 1, 99, 24},
    {1, 107, 28, 5, 108, 28},
    {5, 120, 30, 1, 121, 30},
    {3, 113, 28, 4, 114, 28},
    {3, 107, 28, 5, 108, 28},
    {4, 116, 28, 4, 117, 28},
    {2, 111, 28, 7, 112, 28},
    {4, 121, 30, 5, 122, 30},
    {6, 117, 30, 4, 118, 30},
    {8, 106, 26, 4, 107, 26},
    {10, 114, 28, 2, 115, 28},
    {8, 122, 30, 4, 123, 30},
	{3, 117, 30, 10, 118, 30},
    {7, 116, 30, 7, 117, 30},
    {5, 115, 30, 10, 116, 30},
    {13, 115, 30, 3, 116, 30},
    {17, 115, 30, 0, 0, 0},
    {17, 115, 30, 1, 116, 30},
    {13, 115, 30, 6, 116, 30},
    {12, 121, 30, 7, 122, 30},
    {6, 121, 30, 14, 122, 30},
    {17, 122, 30, 4, 123, 30},
    {4, 122, 30, 18, 123, 30},
    {20, 117, 30, 4, 118, 30},
    {19, 118, 30, 6, 119, 30}
};


/* Caracteristicas de los bloques de error correction para nivel M
   Tablas 13 a 22 del estandar
   numero de bloques, bytes de datos, bytes de error correction, numero de bloques, bytes de datos, bytes de error correction */
static const unsigned int error_correction_character_M [40][6] =
    {
        {1, 16, 10, 0, 0, 0 },
        {1, 28, 16, 0, 0, 0 },
        {1, 44, 26, 0, 0, 0 },
        {2, 32, 18, 0, 0, 0 },
        {2, 43, 24, 0, 0, 0 },
        {4, 27, 16, 0, 0, 0 },
        {4, 31, 18, 0, 0, 0 },
        {2, 38, 22, 2, 39, 22 },
        {3, 36, 22, 2, 37, 22 },
        {4, 43, 26, 1, 44, 26 },
        {1, 50, 30, 4, 51, 30 },
        {6, 36, 22, 2, 37, 22 },
        {8, 37, 22, 1, 38, 22 },
        {4, 40, 24, 5, 41, 24 },
        {5, 41, 24, 5, 42, 24 },
        {7, 45, 28, 3, 46, 28 },
        {10, 46, 28, 1, 47, 28 },
        {9, 43, 26, 4, 44, 26 },
        {3, 44, 26, 11, 45, 26 },
        {3, 41, 26, 13, 42, 26 },
        {17, 42, 26, 0, 0, 0 },
        {17, 46, 28, 0, 0, 0 },
        {4, 47, 28, 14, 48, 28 },
        {6, 45, 28, 14, 46, 28 },
        {8, 47, 28, 13, 48, 28 },
        {19, 46, 28, 4, 47, 28 },
        {22, 45, 28, 3, 46, 28 },
        {3, 45, 28, 23, 46, 28 },
        {21, 45, 28, 7, 46, 28 },
        {19, 47, 28, 10, 48, 28 },
        {2, 46, 28, 29, 47, 28 },
        {10, 46, 28, 23, 47, 28 },
        {14, 46, 28, 21, 47, 28 },
        {14, 46, 28, 23, 47, 28 },
        {12, 47, 28, 26, 48, 28 },
        {6, 47, 28, 34, 48, 28 },
        {29, 46, 28, 14, 47, 28 },
        {13, 46, 28, 32, 47, 28 },
        {40, 47, 28, 7, 48, 28 },
        {18, 47, 28, 31, 48, 28 }
    };

/* Tabla para calcular las coordenadas de los aligment patterns
  (Anexo E del estandar) */
static const uint coordenadas_alignment_patterns[40][8] =
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
    { 4, 6, 34, 62, 90, 0, 0, 0},
    { 5, 6, 28, 50, 72, 94, 0, 0},
    { 5, 6, 26, 50, 74, 98, 0, 0},
    { 5, 6, 30, 54, 78, 102, 0, 0},
    { 5, 6, 28, 54, 80, 106, 0, 0},
    { 5, 6, 32, 58, 84, 110, 0, 0},
    { 5, 6, 30, 58, 86, 114, 0, 0},
    { 5, 6, 34, 62, 90, 118, 0, 0},
    { 6, 6, 26, 50, 74, 98, 122, 0},
    { 6, 6, 30, 54, 78, 102, 126, 0},
    { 6, 6, 26, 52, 78, 104, 130, 0},
    { 6, 6, 30, 56, 82, 108, 134, 0},
    { 6, 6, 34, 60, 86, 112, 138, 0},
    { 6, 6, 30, 58, 86, 114, 142, 0},
    { 6, 6, 34, 62, 90, 118, 146, 0},
    { 7, 6, 30, 54, 78, 102, 126, 150},
    { 7, 6, 24, 50, 76, 102, 128, 154},
    { 7, 6, 28, 54, 80, 106, 132, 158},
    { 7, 6, 32, 58, 84, 110, 136, 162},
    { 7, 6, 26, 54, 82, 110, 138, 166},
    { 7, 6, 30, 58, 86, 114, 142, 170}
};

/* Informacion de la version (Pagina 79 del estandar) */
static bool version_information[34] = { 0x07C94, 0x085BC, 0x09A99, 0x0A4D3, 0x0BBF6, 0x0C762, 0x0D847,
                                         0x0E60D, 0x0F928, 0x10B78, 0x1145D, 0x12A17, 0x13532, 0x149A6,
                                         0x15683, 0x168C9, 0x177EC, 0x18EC4, 0x191E1, 0x1AFAB, 0x1B08E,
                                         0x1CC1A, 0x1D33F, 0x1ED75, 0x1F250, 0x209D5, 0x216F0, 0x228BA,
                                         0x2379F, 0x24B0B, 0x2542E, 0x26A64, 0x27541, 0x28C69 };
                                         
//***********************************************************************/
// FUNCIONES LOCALES                                                    */
//***********************************************************************/
static	uchar 	gadd							    (uchar a, uchar b);
static	uchar 	gmul							    (uchar a, uchar b);
static	uint 	prodQR							    (uint x, uint y);
static	void 	calcula_error_codes					(uchar *data, uint num_data, uchar *error_codes, uint num_error_correction_codes);
static	void 	QRdata_encode_alphanumeric			(struct QRcode *qrcode, char *data, uint data_length);
static	void 	QRdata_encode_numeric				(struct QRcode *qrcode, char *data, uint data_length);
static	void 	QRdata_encode_bytes					(struct QRcode *qrcode, char *data, uint data_length);
static	void 	QRgenera_bitmap						(struct QRcode *qrcode);
static	void 	QRaplica_mascara					(struct QRcode *qrcode, uchar mascara);
 #if 0
static	int 	QRcode_evalua_mascara				(struct QRcode *qrcode, uint min_penalizacion);
static	int 	busqueda_cuadrado_2x2				(struct QRcode *qrcode, int x, int y, uchar xyValue);
static	int 	busqueda_1_1_3_1_1_h				(struct QRcode *qrcode, int x, int y, uchar xyValue);
static	int 	busqueda_1_1_3_1_1_v				(struct QRcode *qrcode, int x, int y, uchar xyValue);
 #endif
static	void 	setPixelAt							(int x, int y, uchar valor, struct BitmapQR *bm);
static	void 	QRgen_error_codes					(struct QRcode *qrcode);
static	void 	genera_indices_datos				(int num_blocks_1, int num_codewords_1, int num_blocks_2, int num_codewords_2, int *result);
static	uchar 	ascii2alphanumeric					(uchar c);
static	int 	busca_version_necesaria				(uchar error_level, uint longitud, uint tipo_codificacion);
static	void 	QRdata_encode						(struct QRcode *qrcode, char *datos, uint longitud, uchar error_level);
static	void 	init_bitStringQR					(struct BitStringQR *bs);
static	void 	_insert_n_bitsQR					(struct BitStringQR *bs, uchar data, uint nbits);
static	void 	insert_n_bitsQR						(struct BitStringQR *bs, uint data, uint nbits);
static	void 	QRcode_init							(struct QRcode *qrcode, uint version, uint error_level);
static	void 	inicializa_bitmap					(int size, struct BitmapQR *bm);
static	void 	colocar_finder_patterns				(struct BitmapQR *bm, int modo);
static	void 	colocar_alignment_patterns			(struct BitmapQR *bm, int modo);
static	void 	colocar_timing_pattern				(struct BitmapQR *bm, int modo);
static	void 	QRcoloca_version_information		(struct QRcode *qrcode);
static	void 	siguiente_libre						(struct BitmapQR *bm, int *x, int *y, int *ud);
static	void 	aplica_mascara_000					(struct BitmapQR *bm, struct BitmapQR *colisiones);
static	void 	aplica_mascara_001					(struct BitmapQR *bm, struct BitmapQR *colisiones);
static	void 	aplica_mascara_010					(struct BitmapQR *bm, struct BitmapQR *colisiones);
static	void 	aplica_mascara_011					(struct BitmapQR *bm, struct BitmapQR *colisiones);
static	void 	aplica_mascara_100					(struct BitmapQR *bm, struct BitmapQR *colisiones);
static	void 	aplica_mascara_101					(struct BitmapQR *bm, struct BitmapQR *colisiones);
static	void 	aplica_mascara_110					(struct BitmapQR *bm, struct BitmapQR *colisiones);
static	void 	aplica_mascara_111					(struct BitmapQR *bm, struct BitmapQR *colisiones);
static	void 	QRcoloca_format_info				(struct QRcode *qrcode);
static	uchar 	bitAtByte							(uchar dato, uchar pos);
static	uchar 	getPixelAt							(int x, int y, struct BitmapQR *bm);

//***********************************************************************/
// VARIABLES LOCALES                                                    */
//***********************************************************************/
uchar	versionLastQR;
static	int		QRbuffer_indices[4096];
struct	QRcode	qrcod;




// *********************************************************************************
// static unsigned char gadd(unsigned char a, unsigned char b)
//
//Suma en el campo de Galoise
//********************************************************************************** */
static uchar gadd(uchar a, uchar b)
{
        return a ^ b;
}

// *********************************************************************************
// static unsigned char gmul(unsigned char a, unsigned char b)
//
//Producto en el campo de Galoise
//********************************************************************************** */
static uchar gmul(uchar a, uchar b)
{
        uchar p = 0;
        uchar counter;
        uchar hi_bit_set;
        for (counter = 0; counter < 8; counter++) {
                if (b & 1)
                        p ^= a;
                hi_bit_set = (a & 0x80);
                a <<= 1;
                if (hi_bit_set)
                        a ^= 0x1d; /* x^8 + x^4 + x^3 + x^2 + 1 */
                b >>= 1;
        }
        return p;
}

// *********************************************************************************
// char prodQR(int x, int y)
//
// Multiplicación en el campo de Galoise para Reed-Solomon
//********************************************************************************** */
static uint prodQR(uint x, uint y) 
{
	unsigned z = 0;
	int i;
	for (i = 7; i >= 0; i--) {
		z = (z << 1) ^ ((z >> 7) * 0x11D);
		z ^= ((y >> i) & 1) * x;
	}
	return z;
}

// *********************************************************************************
// static void calcula_error_codes(unsigned char *data, unsigned int num_data, unsigned char *error_codes, unsigned int num_error_correction_codes)
//
// Funcion que calcula un bloque de correccion de errores
// Pagina 45 del estandar
// 		*data: puntero al inicio de los datos de los que calcular los error codes
// 		num_data: numero de datos en *data
// 		*error_codes: puntero a la zona donde se dejaran los error codes calculados
// 		num_error_correction_codes: numero total de error codes que se van a calcular
//********************************************************************************** */
static void calcula_error_codes(uchar *data, uint num_data, uchar *error_codes, uint num_error_correction_codes)
{
    int i, j;
    uchar c[68], b[68];
    int root=1;

    for (i = 0; i < 68; i++)
      {
      c[i] = 0;
      b[i] = 0;
      }

    c[0] = 1;
    
    for (i = num_error_correction_codes - 1; i >= 0; i--)	/* Calculo de coeficientes */
      {
      for (j = num_error_correction_codes - 1; j >= 0; j--)
	    {
	    c[j] = prodQR (c[j], root);
	    if (j - 1 >= 0)
	        c[j] ^= c[j - 1];
        }
      root = prodQR (root, 0x02);
      }
    

    for (i = 0; i < num_data; i++)
      {
      uchar b65masin;
      int j;

      b65masin = gadd(b[num_error_correction_codes-1], data[i]);
      for (j = num_error_correction_codes-1; j > 0; j--)
         {
         b[j] = gadd(gmul(c[j], b65masin), b[j-1]);            
         ResetWatchDog();
         }
      b[0] = gmul(c[0], b65masin);
      }

    for (i = 0; i < num_error_correction_codes; i++)
        error_codes[i] = b[num_error_correction_codes - 1 - i];
}

//**********************************************************************************
// void QRcode_init(struct QRcode *qrcode, unsigned int version, unsigned int error_level)
//
// Funcion que realiza las tareas de inicialización de un codigo QR
// *qrcode: codigo a inicializar
// version: numero de version del codigo (1 a 40)
// error_level: nivel de deteccion de errores en el codigo: 0 => L, 1 => M, 2 => Q, 3 => H
//**********************************************************************************
static void QRcode_init(struct QRcode *qrcode, uint version, uint error_level)
{
	if( (version > 0 && version <= 40 && error_level <= 1) == 0 )
        return;

    if (versionLastQR != version)
      {
      ini_datamatrix();       /* vamos a utilizar las mismas tablas que el datamatrix */
      }

    qrcode->version = version;
    qrcode->error_level = error_level;

    inicializa_bitmap(21 + ((version-1)*4), &qrcode->codigo);
    inicializa_bitmap(21 + ((version-1)*4), &qrcode->colisiones);
    init_bitStringQR(&qrcode->bs);

    qrcode->num_codewords = tbl_codewords_version[version-1][error_level];
    qrcode->num_error_codewords = error_correction_codewords[version-1][error_level];

    switch(error_level)
    {
        case 0: qrcode->numero_bloques_1 = error_correction_character_L[version-1][0];
                qrcode->num_datos_bloques_1 = error_correction_character_L[version-1][1];
                qrcode->num_error_bloques_1 = error_correction_character_L[version-1][2];

                qrcode->numero_bloques_2 = error_correction_character_L[version-1][3];
                qrcode->num_datos_bloques_2 = error_correction_character_L[version-1][4];
                qrcode->num_error_bloques_2 = error_correction_character_L[version-1][5];
                break;
        case 1: qrcode->numero_bloques_1 = error_correction_character_M[version-1][0];
                qrcode->num_datos_bloques_1 = error_correction_character_M[version-1][1];
                qrcode->num_error_bloques_1 = error_correction_character_M[version-1][2];

                qrcode->numero_bloques_2 = error_correction_character_M[version-1][3];
                qrcode->num_datos_bloques_2 = error_correction_character_M[version-1][4];
                qrcode->num_error_bloques_2 = error_correction_character_M[version-1][5];
                break;
    }

    switch(error_level)
    {
        case 0: qrcode->error_level_code = 1; break;
        case 1: qrcode->error_level_code = 0; break;
        default: break;
    }

    colocar_timing_pattern(&qrcode->codigo, 0);
    colocar_alignment_patterns(&qrcode->codigo, 0);
    colocar_finder_patterns(&qrcode->codigo, 0);

    colocar_timing_pattern(&qrcode->colisiones, 1);
    colocar_alignment_patterns(&qrcode->colisiones, 1);
    colocar_finder_patterns(&qrcode->colisiones, 1);
}

// *********************************************************************************
// static void QRgenera_bitmap(struct QRcode *qrcode)
//
// Funcion que genera el mapa de bits del codigo QR
//********************************************************************************* */
static void QRgenera_bitmap(struct QRcode *qrcode)
{
    int i,j;
    int ud = 0;
    int x = qrcode->codigo.size -1;
    int y = qrcode->codigo.size -1;

    genera_indices_datos(qrcode->numero_bloques_1, qrcode->num_datos_bloques_1, qrcode->numero_bloques_2, qrcode->num_datos_bloques_2, QRbuffer_indices);
    genera_indices_datos(qrcode->numero_bloques_1, qrcode->num_error_bloques_1, qrcode->numero_bloques_2, qrcode->num_error_bloques_2, &QRbuffer_indices[qrcode->num_codewords]);

    for(i = 0; i < qrcode->num_codewords; i++)
    {
       uchar valor;
       valor = qrcode->bs.datos[QRbuffer_indices[i]];
       for(j = 7; j >= 0; j--)
          {
            uchar aux_bit;
            aux_bit = bitAtByte(valor, j);
            setPixelAt(x, y, aux_bit, &qrcode->codigo);
			siguiente_libre(&qrcode->colisiones, &x, &y, &ud);
			ResetWatchDog();
          }
    }

    for(i = qrcode->num_codewords; i < qrcode->num_codewords + qrcode->num_error_codewords; i++)
    {
        uchar valor;
        unsigned aux_buf_i;
        aux_buf_i =  QRbuffer_indices[i];
        valor = qrcode->bs.datos[qrcode->num_codewords + aux_buf_i];
        for(j = 7; j >= 0; j--)
        {
            setPixelAt(x, y, bitAtByte(valor, j), &qrcode->codigo);
			siguiente_libre(&qrcode->colisiones, &x, &y, &ud);
			ResetWatchDog();
        }
    }
}

// *********************************************************************************
// static void QRaplica_mascara(struct QRcode *qrcode, unsigned char mascara)
// 
// Funcion que aplica una de las 8 posibles mascaras al bitmap del codigo QR
// mascara: 0 a 7
//********************************************************************************* */
static void QRaplica_mascara(struct QRcode *qrcode, uchar mascara)
{
    qrcode->mascara = mascara;
    qrcode->format_info = qrcode->error_level_code << 3 | qrcode->mascara;
    switch(mascara)
    {
        case 0: 
            aplica_mascara_000(&qrcode->codigo, &qrcode->colisiones);
        break;
        case 1: 
            aplica_mascara_001(&qrcode->codigo, &qrcode->colisiones);
        break;
        case 2: 
            aplica_mascara_010(&qrcode->codigo, &qrcode->colisiones);
        break;
        case 3: 
            aplica_mascara_011(&qrcode->codigo, &qrcode->colisiones);
        break;
        case 4: 
            aplica_mascara_100(&qrcode->codigo, &qrcode->colisiones);
        break;
        case 5: 
            aplica_mascara_101(&qrcode->codigo, &qrcode->colisiones);
        break;
        case 6: 
            aplica_mascara_110(&qrcode->codigo, &qrcode->colisiones);
        break;
        case 7: 
            aplica_mascara_111(&qrcode->codigo, &qrcode->colisiones);
        break;
        default: 
        break;
    }
}
 #if 0
// *********************************************************************************
// static int busqueda_cuadrado_2x2(struct QRcode *qrcode, int x, int y)
//
// Funcion que evalua si en la posición (x,y) del bitmap del codigo QR
// existe o no un cuadrado de 2x2 pixels del mismo color.
// Devuelve 1 si existe, 0 si no existe
//********************************************************************************* */
static int busqueda_cuadrado_2x2(struct QRcode *qrcode, int x, int y, uchar xyValue)
{
    if(getPixelAt(x + 1, y, &qrcode->codigo) == xyValue)
        if(getPixelAt(x, y + 1, &qrcode->codigo) == xyValue)
            if(getPixelAt(x + 1, y + 1, &qrcode->codigo) == xyValue)
                return 1;
    return 0;
}

// *********************************************************************************
// static int busqueda_1_1_3_1_1_h(struct QRcode *qrcode, int x, int y)
//
// Funcion que evalua si a partir de la posicion de inicio (x,y) hay un patron
// 1:1:3:1:1 (negro, blanco, negro, blanco, negro) de pixels, en horizontal
// Devuelve 1 si existe, 0 si no existe
//********************************************************************************* */
static int busqueda_1_1_3_1_1_h(struct QRcode *qrcode, int x, int y, uchar xyValue)
{
    if( xyValue == 1 )
        if(getPixelAt(x+1, y,&qrcode->codigo) == 0)
            if(getPixelAt(x+2, y,&qrcode->codigo) == 1)
                if(getPixelAt(x+3, y,&qrcode->codigo) == 1)
                    if(getPixelAt(x+4, y,&qrcode->codigo) == 1)
                        if(getPixelAt(x+5, y,&qrcode->codigo) == 0)
                            if(getPixelAt(x+6, y,&qrcode->codigo) == 1)
                                return 1;

    return 0;
}

// *********************************************************************************
// static int busqueda_1_1_3_1_1_v(struct QRcode *qrcode, int x, int y)
// Funcion que evalua si a partir de la posicion de inicio (x,y) hay un patron
// 1:1:3:1:1 (negro, blanco, negro, blanco, negro) de pixels, en vertical
// Devuelve 1 si existe, 0 si no existe
//********************************************************************************* */
static int busqueda_1_1_3_1_1_v(struct QRcode *qrcode, int x, int y, uchar xyValue)
{
    if( xyValue == 1 )
        if(getPixelAt(x, y+1,&qrcode->codigo) == 0)
            if(getPixelAt(x, y+2,&qrcode->codigo) == 1)
                if(getPixelAt(x, y+3,&qrcode->codigo) == 1)
                    if(getPixelAt(x, y+4,&qrcode->codigo) == 1)
                        if(getPixelAt(x, y+5,&qrcode->codigo) == 0)
                            if(getPixelAt(x, y+6,&qrcode->codigo) == 1)
                                return 1;

    return 0;
}

// *********************************************************************************
// static int QRcode_evalua_mascara(struct QRcode *qrcode)
//
// Funcion que calcula el factor de penalizacion del codigo QR con la mascara que
// tiene activa en ese momento.
// Devuelve el factor de penalización.
//********************************************************************************* */
static int QRcode_evalua_mascara(struct QRcode *qrcode, uint min_penalizacion)
{
    int x, y;
    uint penalizacion = 0;
    uchar anterior;
    uchar actual;
    uint encontrados = 0;
    uint cuantos_negros = 0;
    ulong porcentaje_negros = 0;

    // Busco grupos de mas de 5 bloques del mismo color en vertical
    //Penalizacion: 3 + 1 por cada pixel a partir de 5
    //Aprovecho para contar el numero de pixels en negro */ 
    for(x = 0; x < qrcode->codigo.size - 1; x++)
    {
    	if (min_penalizacion < penalizacion)	/* Si ya hemos sobrepasado la penalización óptima, no continuamos. */
    		return (1000000);
    		
        anterior = 2;
        actual = 2;
        encontrados = 0;

        for(y = 0; y < qrcode->codigo.size - 1; y++)
        {
            actual = getPixelAt(x, y, &qrcode->codigo);            
            cuantos_negros += actual;
            
            /* Busqueda 1:1:3:1:1 en vertical
    		   Penalizacion: 40 por cada vez que se encuentre */
        	if (y < qrcode->codigo.size - 5)
        	{
                if(busqueda_1_1_3_1_1_v(qrcode, x, y, actual))
                    penalizacion += 40;			
        	}
        	
        	/* Busqueda de cuadrados de 2x2
			   Penalizacion: 3 por cada uno que se encuentre */
            if(busqueda_cuadrado_2x2(qrcode, x, y, actual))
                penalizacion += 3;  
            
            if(actual == anterior)
            {
                encontrados++;
            }
            else
            {
                if(encontrados >= 5)
                    penalizacion += (3 + (5 - encontrados));
                encontrados = 0;
            }
            anterior = actual;			          
        }
    }
    ResetWatchDog();
    /* Busco grupos de mas de 5 bloques del mismo color en horizontal
       Penalizacion: 3 + 1 por cada pixel a partir de 5 */
    for(y = 0; y < qrcode->codigo.size - 1; y++)
    {
    	if (min_penalizacion < penalizacion)	/* Si ya hemos sobrepasado la penalización óptima, no continuamos. */
    		return (1000000);

        anterior = 2;
        actual = 2;
        encontrados = 0;
        for(x = 0; x < qrcode->codigo.size - 1; x++)
        {
            actual = getPixelAt(x, y, &qrcode->codigo);
            
            /* Busqueda 1:1:3:1:1 en horizontal
    		   Penalizacion: 40 por cada vez que se encuentre */
    		if (x < qrcode->codigo.size - 5)
    		{    			
                if(busqueda_1_1_3_1_1_h(qrcode, x, y, actual))
                    penalizacion += 40;
            }  
            
            if(actual == anterior)
            {
                encontrados++;
            }
            else
            {
                if(encontrados >= 5)
                    penalizacion += (3 + (5 - encontrados));
                encontrados = 0;
            }
            anterior = actual;                      
        }
    }
    ResetWatchDog();
    /* Porcentaje de negros
       Penalizacion: 10 por cada salto de 5% a partir de 45% y 55% */
    porcentaje_negros = (1000 * cuantos_negros) / ((qrcode->codigo.size)*(qrcode->codigo.size));
    if(porcentaje_negros > 450 && porcentaje_negros < 550)
        x = 0;
    else if(porcentaje_negros > 400 && porcentaje_negros < 600)
        x = 1;
    else if(porcentaje_negros > 350 && porcentaje_negros < 650)
        x = 2;
    else if(porcentaje_negros > 300 && porcentaje_negros < 700)
        x = 3;
    else if(porcentaje_negros > 250 && porcentaje_negros < 750)
        x = 4;
    else if(porcentaje_negros > 200 && porcentaje_negros < 800)
        x = 5;
    else if(porcentaje_negros > 150 && porcentaje_negros < 850)
        x = 6;
    else if(porcentaje_negros > 100 && porcentaje_negros < 900)
        x = 7;
    else if(porcentaje_negros > 50 && porcentaje_negros < 950)
        x = 8;
    else
        x = 9;
    penalizacion += x*10;
    
    return penalizacion;
}
 #endif

// *********************************************************************************
// void QRcalcula_codigo_optimo(struct QRcode *qrcode, char *datos)
//
// Funcion que genera el bitmap completo de un codigo QR
// Prueba todas las mascaras posibles y escoge la que obtiene menor valor de penalizacion
// *datos: string con los datos a codificar en el codigo QR
//********************************************************************************* */
void QRcalcula_codigo_optimo(struct QRcode *qrcode, char *datos, uint longitud, uchar error_level)
{
 #if 0
    int i;
    int min_penalizacion;
    int mascara_min_penalizacion;    
 #endif
  
    QRdata_encode(qrcode, datos, longitud, error_level);
    ResetWatchDog();
    QRgen_error_codes(qrcode);
    ResetWatchDog();
    QRgenera_bitmap(qrcode);
    ResetWatchDog();
 #if 0
    min_penalizacion = 1000000;

	mascara_min_penalizacion = 0;
    for(i = 0; i <= 7; i++)
    {
        int penalizacion;
        QRaplica_mascara(qrcode, i);
        penalizacion = QRcode_evalua_mascara(qrcode, min_penalizacion);
        if(penalizacion < min_penalizacion)
        {
            min_penalizacion = penalizacion;
            mascara_min_penalizacion = i;
        }
        QRaplica_mascara(qrcode, i);
    }
    
    QRaplica_mascara(qrcode, mascara_min_penalizacion);
 #endif
 
    QRaplica_mascara(qrcode, 3);
    ResetWatchDog();
    QRcoloca_format_info(qrcode);
    QRcoloca_version_information(qrcode);
    ResetWatchDog();
}

// *********************************************************************************
// static void QRgen_error_codes(struct QRcode *qrcode)
//
// Función que calcula todos los bloques de error codes
//********************************************************************************* */
static void QRgen_error_codes(struct QRcode *qrcode)
{
    int i;

    uchar *data;
    uchar *error_codes;

    data = &qrcode->bs.datos[0]; //data apunta al comienzo del array de bytes con los datos
    error_codes = data + qrcode->num_codewords; //error_codes apunta a la posición del array de datos a partir de la cual hay que colocar los error codes

    for(i = 0; i < qrcode->numero_bloques_1; i++)
    {
        calcula_error_codes(data, qrcode->num_datos_bloques_1, error_codes, qrcode->num_error_bloques_1);
        data += qrcode->num_datos_bloques_1;
        error_codes += qrcode->num_error_bloques_1;
        qrcode->bs.length += 8*qrcode->num_error_bloques_1; //Actualizo el tamaño del bitstring
		ResetWatchDog(); //CORRIGE RESETEO
    }
    for(i = 0; i < qrcode->numero_bloques_2; i++)
    {
        calcula_error_codes(data, qrcode->num_datos_bloques_2, error_codes, qrcode->num_error_bloques_2);
        data += qrcode->num_datos_bloques_2;
        error_codes += qrcode->num_error_bloques_2;
        qrcode->bs.length += 8*qrcode->num_error_bloques_2; //Actualizo el tamaño del bitstring
		ResetWatchDog(); //CORRIGE RESETEO
    }
}

// *********************************************************************************
// static void genera_indices_datos(int num_blocks_1, int num_codewords_1, int num_blocks_2, int num_codewords_2, unsigned int *result)
//
//Funcion que genera el orden en el que hay que insertar los bloques de datos en el simbolo
//Devuelve el resultado en *result. Puede llegar a ocupar +3000 bytes.
//Los argumentos se sacan de las tablas 13 a 21 del estandar (tablas de nombre error_correction_characteristics_XXXX en el codigo)
//********************************************************************************* */
static void genera_indices_datos(int num_blocks_1, int num_codewords_1, int num_blocks_2, int num_codewords_2, int *result)
{
    int i;

    int salidas[100]; // Maximo hay 81 en version 40 y nivel de errores H
    int max_salidas[100];

    int indice = 0;
    int max_indice ;

    max_indice = num_blocks_1 + num_blocks_2;

    salidas[0] = 0;
    max_salidas[0] = num_codewords_1;
    for(i = 1; i < num_blocks_1; i++)
    {
        salidas[i] = salidas[i-1] + num_codewords_1;
        max_salidas[i] = max_salidas[i-1] + num_codewords_1;
        ResetWatchDog();
    }
    
    ResetWatchDog();
    
    salidas[i] = salidas[i-1] + num_codewords_1;
    max_salidas[i] = max_salidas[i-1] + num_codewords_2;
    for(i = num_blocks_1 + 1; i < num_blocks_1 + num_blocks_2; i++)
    {
        salidas[i] = salidas[i-1] + num_codewords_2;
        max_salidas[i] = max_salidas[i-1] + num_codewords_2;
        ResetWatchDog();
    }
    
    ResetWatchDog();
    
    i = 0;
    while(1)
    {
        if(salidas[max_indice-1] == max_salidas[max_indice-1])
            break;
        if(salidas[indice] < max_salidas[indice])
        {
            result[i] = salidas[indice];
            //printf("result[%d] = %d", i, result[i]);
            i++;
        }
        salidas[indice] += 1;
        indice +=1;
        if(indice == max_indice)
            indice = 0;
        ResetWatchDog();
    }
}

//**********************************************************************************
// static unsigned char ascii2alphanumeric(unsigned char c)
//
// Codifica el caracter c, con la codificacion alfanumerica. (Tabla 5 del estandar)
//**********************************************************************************
static uchar ascii2alphanumeric(uchar c)
{
    if(c >= 0x30 && c <= 0x39)
    {
        c = c - 0x30;
    }
    else if(c >= 0x41 && c <= 0x5A)
    {
        c = c - 0x37;
    }
    else
    {
        switch(c)
        {
            case 0x20: c = 36; break;
            case 0x24: c = 37; break;
            case 0x25: c = 38; break;
            case 0x2A: c = 39; break;
            case 0x2B: c = 40; break;
            case 0x2D: c = 41; break;
            case 0x2E: c = 42; break;
            case 0x2F: c = 43; break;
            case 0x3A: c = 44; break;
        }
    }
    return c;
}

//**********************************************************************************
// static void QRdata_encode_alphanumeric(struct QRcode *qrcode, char *data)
//
// Codifica el string *data utilizando el modo alfanumerico
//**********************************************************************************
static void QRdata_encode_alphanumeric(struct QRcode *qrcode, char *data, uint data_length)
{
    int i;
    unsigned char pad[2] = {0xEC, 0x11};

    unsigned int cuantos_pares, ultimo;
    unsigned int length_padding;



    unsigned int version;
    unsigned int n_code_data;
    unsigned int char_count_indicator;

    unsigned char caracter1, caracter2;

    struct BitStringQR *bs;



    version = qrcode->version;
    n_code_data = qrcode->num_codewords;
    bs = &qrcode->bs;

    if(version < 10)
        char_count_indicator = 9;
    else if(version < 27)
        char_count_indicator = 11;
    else
        char_count_indicator = 13;

    insert_n_bitsQR(bs, 0x2, 4);
	ResetWatchDog();
    insert_n_bitsQR(bs, data_length, char_count_indicator);
	ResetWatchDog();

    cuantos_pares = data_length / 2;
    ultimo = data_length % 2;

    for(i = 0; i < cuantos_pares; i++)
    {
        unsigned int aux;
        caracter1 = data[i*2];
        caracter2 = data[1+i*2];
        caracter1 = ascii2alphanumeric(caracter1);
        caracter2 = ascii2alphanumeric(caracter2);
        aux = (caracter1 * 45) + caracter2;
        insert_n_bitsQR(bs, aux, 11);
		ResetWatchDog(); //CORRIGE RESETEO
    }
    //Si hay un ultimo caracter suelto
    if(ultimo == 1)
    {
        caracter1 = (data[i*2]);
        caracter1 = ascii2alphanumeric(caracter1);
        insert_n_bitsQR(bs, caracter1, 6);
    }

    //Terminator
    {
        unsigned int cuantos_bits_quedan;
        cuantos_bits_quedan = (n_code_data * 8) - bs->length;
        //Si quedan menos de 4 bits para completar el tamaño maximo del simbolo, el terminador no se introduce completo
        if(cuantos_bits_quedan < 4)
        {
            insert_n_bitsQR(bs, 0, cuantos_bits_quedan);
        }
        else
        {
            insert_n_bitsQR(bs, 0, 4);
        }
    }

    //Padding de 0s
    if(bs->length%8 != 0)
        insert_n_bitsQR(bs, 0, 8 - bs->length%8);

    //Padding
    length_padding = bs->length/8;
    for(i = 0; i < n_code_data - length_padding; i++)
        insert_n_bitsQR(bs, pad[i%2], 8);

}

//**********************************************************************************
// static void QRdata_encode_numeric(struct QRcode *qrcode, char *data)
//
//Codifica en modo numerico el string *data (que solo debe contener numeros)
//**********************************************************************************
static void QRdata_encode_numeric(struct QRcode *qrcode, char *data, uint data_length)
{
    int i;
    unsigned char pad[2] = {0xEC, 0x11};

    unsigned int cuantos_trios, ultimo;
    unsigned int length_padding;



    unsigned int version;
    unsigned int n_code_data;
    unsigned int char_count_indicator;

    struct BitStringQR *bs;


    version = qrcode->version;
    n_code_data = qrcode->num_codewords;
    bs = &qrcode->bs;

    if(version < 10)
        char_count_indicator = 10;
    else if(version < 27)
        char_count_indicator = 12;
    else
        char_count_indicator = 14;

    insert_n_bitsQR(bs, 0x1, 4);
    insert_n_bitsQR(bs, data_length, char_count_indicator);

    cuantos_trios = data_length / 3;
    ultimo = data_length % 3;

    for(i = 0; i < cuantos_trios; i++)
    {
        unsigned int aux;
        //El menos 0x30 es para pasar de ASCII a digito binario
        aux = (data[i*3 + 0] - 0x30)*100 + (data[i*3 + 1] - 0x30)*10 + (data[i*3 + 2] - 0x30)*1;
        insert_n_bitsQR(bs, aux, 10);
		ResetWatchDog(); //CORRIGE RESETEO
    }

    if(ultimo == 2)
    {
        unsigned int aux;
        //El menos 0x30 es para pasar de ASCII a digito binario
        aux = (data[i*3 + 0] - 0x30)*10 + (data[i*3 + 1] - 0x30)*1;
        insert_n_bitsQR(bs, aux, 7);
    }

    if(ultimo == 1)
    {
        unsigned int aux;
        //El menos 0x30 es para pasar de ASCII a digito binario
        aux = (data[i*3 + 0] - 0x30)*1;
        insert_n_bitsQR(bs, aux, 4);
    }

    //Terminator
    {
        unsigned int cuantos_bits_quedan;
        cuantos_bits_quedan = (n_code_data * 8) - bs->length;
        //Si quedan menos de 4 bits para completar el tamaño maximo del simbolo, el terminador no se introduce completo
        if(cuantos_bits_quedan < 4)
        {
            insert_n_bitsQR(bs, 0, cuantos_bits_quedan);
        }
        else
        {
            insert_n_bitsQR(bs, 0, 4);
        }
    }

    //Padding de 0s
    if(bs->length%8 != 0)
        insert_n_bitsQR(bs, 0, 8 - bs->length%8);

    //Padding
    length_padding = bs->length/8;
    for(i = 0; i < n_code_data - length_padding; i++)
        {
        insert_n_bitsQR(bs, pad[i%2], 8);
        ResetWatchDog();
        }

}


// *********************************************************************************
// static void QRdata_encode_bytes(struct QRcode *qrcode, char *data, unsigned int longitud)
//
// Codifica el string *data utilizando el modo bytes
//********************************************************************************* */
static void QRdata_encode_bytes(struct QRcode *qrcode, char *data, uint data_length)
{
    int i;
   unsigned char pad[2] = {0xEC, 0x11};


    unsigned int length_padding;



    unsigned int version;
    unsigned int n_code_data;
    unsigned int char_count_indicator;

    unsigned char caracter1;

    struct BitStringQR *bs;


    version = qrcode->version;
    n_code_data = qrcode->num_codewords;
    bs = &qrcode->bs;
    
#if 0
 #if QR_ECI_OPCIONAL
	if ( ObtenValorAjuste(ID_AJ_CONF_QRCODE_ECI) != 0)
 #endif		
 	{
		insert_n_bitsQR(bs, 0x7, 4); //ECI
	#if CORRECCIONES_QRCODE_4
		insert_n_bitsQR(bs, ObtenQR_ECI(), 8);
	#else
		insert_n_bitsQR(bs, 0x03, 8);
	#endif		
	}
#endif
	
	if(version < 10)
        char_count_indicator = 8;
    else
        char_count_indicator = 16;

    insert_n_bitsQR(bs, 0x4, 4);
    insert_n_bitsQR(bs, data_length, char_count_indicator);

    for(i = 0; i < data_length; i++)
    {
        caracter1 = data[i];
		insert_n_bitsQR(bs, caracter1, 8);
		ResetWatchDog(); //CORRIGE RESETEO
    }

    //Terminator
    {
        uint cuantos_bits_quedan;
        cuantos_bits_quedan = (n_code_data * 8) - bs->length + 12;
        //Si quedan menos de 4 bits para completar el tamaño maximo del simbolo, el terminador no se introduce completo
        if(cuantos_bits_quedan < 4)
        {
            insert_n_bitsQR(bs, 0, cuantos_bits_quedan);
        }
        else
        {
            insert_n_bitsQR(bs, 0, 4);
        }
    }

    //Padding de 0s
    if(bs->length%8 != 0)
        insert_n_bitsQR(bs, 0, 8 - bs->length%8);

    //Padding
    length_padding = bs->length/8;

 #if QR_ECI_OPCIONAL
	if ( ObtenValorAjuste(ID_AJ_CONF_QRCODE_ECI) != 0 )
	{
    	for(i = 0; i < n_code_data + 12 - length_padding; i++) //incluye los 12 del ECI
       	 insert_n_bitsQR(bs, pad[i%2], 8);		
	}
	else
	{
		for(i = 0; i < n_code_data - length_padding; i++)
        	insert_n_bitsQR(bs, pad[i%2], 8);	
	}		
 #else
    for(i = 0; i < n_code_data + 12 - length_padding; i++) //incluye los 12 del ECI
        {
        insert_n_bitsQR(bs, pad[i%2], 8);
        ResetWatchDog();
        }
 #endif        
}

// *********************************************************************************
// static int busca_version_necesaria(unsigned char error_level, unsigned int longitud, unsigned int tipo_codificacion)
//
// Funcion que calcula la version necesaria para un nivel de error, tamaño y tipo de codificacion concretos
// tipo_codificacion: 0 => numerica, 1 => alfanumerica, 2 => bytes
// (Tablas 7 a 11 del estandar)
//********************************************************************************* */
static int busca_version_necesaria(uchar error_level, uint longitud, uint tipo_codificacion)
{
    int i;
    uint longitud_encontrada = 0;
    
    for(i = 0; i < 40; i++)
    {
        switch(error_level)
        {
            case 0: longitud_encontrada = tabla_capacidades_L[i][tipo_codificacion]; break;
            case 1: longitud_encontrada = tabla_capacidades_M[i][tipo_codificacion]; break;
        }
        if(longitud_encontrada >= longitud)
            break;
    }
    return i+1;
}

// *********************************************************************************
// uchar getTipoMsg(char* datos, unsigned int longitud)
//
// Calcula el tipo de datos que hay en el mensaje. Devuelv 0 para númericos, 1 para alfanuméricos y 2 para otros
//********************************************************************************* */
uchar getTipoMsg(char* datos, uint longitud)
{
    uchar bNumerica = 1;
    uchar bAlfaNumerica = 1;
    uint i;

    for (i = 0; (i < longitud) && (bNumerica | bAlfaNumerica); i++)
    {
        if (datos[i] < 0x30 || datos[i] > 0x39)
            bNumerica = 0;
        
        if (datos[i] >= 0x20 && datos[i] <= 0x3A)
            bAlfaNumerica = tablaPermitidos[datos[i] - 0x20];
        else if (datos[i] < 0x41 || datos[i] > 0x5A)
            bAlfaNumerica = 0;
        ResetWatchDog();    
    }

    if (bNumerica)
        return 0;
    else if (bAlfaNumerica)
        return 1;
    else
        return 2;
}

// *********************************************************************************
// static void QRdata_encode(struct QRcode *qrcode, char *datos, unsigned int longitud, unsigned char error_level)
//
// Codifica dentro de la estructura *qrcode el string *datos
//********************************************************************************* */
static void QRdata_encode(struct QRcode *qrcode, char *datos, uint longitud, uchar error_level)
{
    uchar tipoMsg = getTipoMsg(datos, longitud);
    QRcode_init(qrcode, busca_version_necesaria(error_level, longitud, tipoMsg), error_level);
    
    if (tipoMsg == 0)
    {
        QRdata_encode_numeric(qrcode, datos, longitud);
    }
    else if (tipoMsg == 1)
    {
        QRdata_encode_alphanumeric(qrcode, datos, longitud);
    }
    else
    {
        QRdata_encode_bytes(qrcode, datos, longitud);
    }
}

//**********************************************************************************
// static void init_bitStringQR(struct BitStringQR *bs)
//
// Inicializa un bitstring
//**********************************************************************************
static void init_bitStringQR(struct BitStringQR *bs)
{
    int i;
    for(i = 0; i < 3360; i++)
    {
        bs->datos[i] = 0;
        ResetWatchDog(); //CORRIGE RESETEO
    }
    bs->length = 0;

}

//**********************************************************************************
// static void _insert_n_bitsQR(struct BitStringQR *bs, unsigned char data, unsigned int nbits)
//
//inserta un maximo de 8 bits en un bitsring
// data: dato a introducir
// nbits: numero de bits a utilizar
//**********************************************************************************
static void _insert_n_bitsQR(struct BitStringQR *bs, uchar data, uint nbits)
{
    int hueco; //Bits que faltan para rellenar el ultimo int que esta a medias
    int indice_ultimo_int; //Indice del ultimo int que esta a medias
    uint mascara, data_aux, dato_previo;

    if(nbits > 8)
    {
        return;
    }

    hueco = 8 - (bs->length % 8);
    indice_ultimo_int = bs->length / 8;


    //Relleno lo que queda del hueco
    if(nbits > hueco)
    {
        mascara = 0xFF >> (8 - hueco) ;

        data_aux = data >> (nbits - hueco);
        data_aux = data_aux & mascara;

        dato_previo = bs->datos[indice_ultimo_int];
        bs->datos[indice_ultimo_int] = dato_previo | data_aux;
    }
    else
    {
        mascara = (0xFFF >> (8 - nbits));
        mascara = mascara << (hueco - nbits);

        data_aux = data << (hueco - nbits);
        data_aux = data_aux & mascara;

        dato_previo = bs->datos[indice_ultimo_int];
        bs->datos[indice_ultimo_int] = dato_previo | data_aux;

    }

    //Si hay mas bits que meter
    if(nbits > hueco)
    {
        data_aux = data <<  (8 - (nbits - hueco) );
        bs->datos[indice_ultimo_int + 1] = data_aux;
    }

    bs->length += nbits;
}

// *********************************************************************************
// static void insert_n_bitsQR(struct BitStringQR *bs, unsigned int data, unsigned int nbits)
//
//Inserta un maximo de 32 bits en el bitString
// data: dato a introducir
// nbits: numero de bits a utilizar
//********************************************************************************* */
static void insert_n_bitsQR(struct BitStringQR *bs, uint data, uint nbits)
{
    int bytes;
    uint aux_data;
    uchar aux_data_char, mascara;
    int bits_primer_byte;
    int es_primer_byte;

    if(nbits > 32)
        return;

    bits_primer_byte = nbits % 8;
    bytes = 1+ (nbits / 8);
    if(bits_primer_byte == 0)
    {
        bits_primer_byte = 8;
        bytes--;
    }

    es_primer_byte = 1;
    if(bytes == 4)
    {

        aux_data = (data >> 24);
        aux_data_char = (unsigned char)aux_data;

        mascara = (0xFF >> (8 - bits_primer_byte) );

        aux_data_char &= mascara;

        _insert_n_bitsQR(bs, aux_data_char, bits_primer_byte);
        bytes--;
        es_primer_byte = 0;
    }
    if(bytes == 3)
    {
        aux_data = (data >> 16) & 0x000000FF;
        aux_data_char = (unsigned char)aux_data;

        if(es_primer_byte == 1)
        {
            mascara = (0xFF >> (8 - bits_primer_byte) );
            aux_data_char &= mascara;
        }

        _insert_n_bitsQR(bs, aux_data_char, (es_primer_byte ? bits_primer_byte : 8));
        bytes--;
        es_primer_byte = 0;
    }
    if(bytes == 2)
    {
        aux_data = (data >> 8) & 0x000000FF;
        aux_data_char = (unsigned char)aux_data;

        if(es_primer_byte == 1)
        {
            mascara = (0xFF >> (8 - bits_primer_byte) );
            aux_data_char &= mascara;
        }

        _insert_n_bitsQR(bs, aux_data_char, (es_primer_byte ? bits_primer_byte : 8));
        bytes--;
        es_primer_byte = 0;
    }
    if(bytes == 1)
    {
        aux_data = data & 0x000000FF;
        aux_data_char = (unsigned char)aux_data;

        if(es_primer_byte == 1)
        {
            mascara = (0xFF >> (8 - bits_primer_byte) );
            aux_data_char &= mascara;
        }

        _insert_n_bitsQR(bs, aux_data_char, (es_primer_byte ? bits_primer_byte : 8));
        bytes--;
        es_primer_byte = 0;
    }
}

//**********************************************************************************
// static void inicializa_bitmap(int size, struct BitmapQR *bm)
//
// Inicializa un bitmap de tamaño size
//**********************************************************************************
static void inicializa_bitmap(int size, struct BitmapQR *bm)
{
    int i;
    bm->size = size;
    for(i = 0; i < 4100; i++)
	{
		ResetWatchDog();
        bm->m[i] = 0;
	}

}

// *********************************************************************************
// void InviertePixelAt(int x, int y, struct BitmapQR *bm)
//
// Invierte el valor del pixel en la posicion (x,y)
//********************************************************************************* */
static void InviertePixelAt(int x, int y, struct BitmapQR *bm)
{
    int bytes_por_fila;
    int byte_buscado;
    uchar pos_in_byte;

    bytes_por_fila = (bm->size/8) + ((bm->size%8 == 0) ? 0 : 1);
    byte_buscado = y*bytes_por_fila + x/8;    
    
    pos_in_byte = ( (y * bytes_por_fila * 8) + x ) % 8;
    bm->m[byte_buscado] ^= 0x80 >> pos_in_byte;
}

// *********************************************************************************
// unsigned char getPixelAt(int x, int y, struct BitmapQR *bm)
//
// Devuelve el valor del pixel en la posicion (x,y)
//********************************************************************************* */
static uchar getPixelAt(int x, int y, struct BitmapQR *bm)
{
        
    int bytes_por_fila;
    int byte_buscado;
    int pos_in_byte;
    uchar pixel_value;
    uchar aux;

    bytes_por_fila = (bm->size/8) + ((bm->size%8 == 0) ? 0 : 1);
    byte_buscado = y*bytes_por_fila + x/8;    

    pos_in_byte = ( (y * bytes_por_fila * 8) + x ) % 8;
    aux = bm->m[byte_buscado];
    aux &= (0x80 >> pos_in_byte);
    pixel_value = aux != 0 ? 1 : 0;

    return pixel_value;
}

// *********************************************************************************
// static void setPixelAt(int x, int y, unsigned char valor, struct BitmapQR *bm)
//
// Pone el valor del pixel en la posicion (x,y)
// valor: 0 => 0, distinto de 0 => 1
//********************************************************************************* */
static void setPixelAt(int x, int y, uchar valor, struct BitmapQR *bm)
{
    int bytes_por_fila;
    int byte_buscado;
    int pos_in_byte;
    uchar byte_mask;


    bytes_por_fila = (bm->size/8) + ((bm->size%8 == 0) ? 0 : 1);
		byte_buscado = y*bytes_por_fila + x/8;
    //byte_buscado = ( (y * bm->size) + x ) / 8;
    

    //pos_in_byte = ( (y * bm->size) + x ) % 8;
    pos_in_byte = ( (y * bytes_por_fila * 8) + x ) % 8;
    
    if( valor == 0)
    {
        byte_mask = ~(0x80 >> pos_in_byte);        
        bm->m[byte_buscado] &= byte_mask;
    }
    else
    {
        byte_mask = (0x80 >> pos_in_byte);
        bm->m[byte_buscado] |= byte_mask;
    }
}

// *********************************************************************************
// static void colocar_finder_patterns(struct BitmapQR *bm, int modo)
//
// Coloca los finder patterns en el bitmap
//Modo = 0 => Coloca los finder patterns
//Modo = 1 => Coloca todo negro en los finder patterns, para la mascara de colisiones
//********************************************************************************* */
static void colocar_finder_patterns(struct BitmapQR *bm, int modo)
{
    uchar finderPattern[7] = {0xFF, 0x06, 0xED, 0XDB, 0XB0, 0X7F, 0X80};
    int i, j;
    int cuenta_bit;
    int cuenta_byte;
    uchar aux_byte;
    uchar aux_bit;
    int posX, posY;
    uint version;

    version = 1 + ((bm->size - 21) / 4);
    //Esquina superior izquierda
    posX = 0;
    posY = 0;
    cuenta_bit = 0;
    cuenta_byte = 0;
    for(i = posX + 0; i < posX + 7; i++)
    {
        for(j = posY + 0; j < posY + 7; j++)
        {
            aux_byte = finderPattern[cuenta_byte];
            aux_bit = ((aux_byte & (0x80 >> cuenta_bit)) != 0) ? 1 : 0;

            if(modo == 0)
                setPixelAt(i, j, aux_bit, bm);
            else if (modo == 1)
                setPixelAt(i, j, 1, bm);
            cuenta_bit++;
            if(cuenta_bit == 8)
            {
                cuenta_bit = 0;
                cuenta_byte++;
            }
			ResetWatchDog();
        }
    }

    //Esquina superior derecha
    posX = bm->size - 7;
    posY = 0;
    cuenta_bit = 0;
    cuenta_byte = 0;
    for(i = posX + 0; i < posX + 7; i++)
    {
        for(j = posY + 0; j < posY + 7; j++)
        {
            aux_byte = finderPattern[cuenta_byte];
            aux_bit = ((aux_byte & (0x80 >> cuenta_bit)) != 0) ? 1 : 0;
            if(modo == 0)
                setPixelAt(i, j, aux_bit, bm);
            else if (modo == 1)
                setPixelAt(i, j, 1, bm);
            cuenta_bit++;
            if(cuenta_bit == 8)
            {
                cuenta_bit = 0;
                cuenta_byte++;
            }
			ResetWatchDog();
        }
    }

    //Esquina inferior izquierda
    cuenta_bit = 0;
    cuenta_byte = 0;
    posX = 0;
    posY = bm->size - 7;
    for(i = posX + 0; i < posX + 7; i++)
    {
        for(j = posY + 0; j < posY + 7; j++)
        {
            aux_byte = finderPattern[cuenta_byte];
            aux_bit = ((aux_byte & (0x80 >> cuenta_bit)) != 0) ? 1 : 0;
            if(modo == 0)
                setPixelAt(i, j, aux_bit, bm);
            else if (modo == 1)
                setPixelAt(i, j, 1, bm);
            cuenta_bit++;
            if(cuenta_bit == 8)
            {
                cuenta_bit = 0;
                cuenta_byte++;
            }
			ResetWatchDog();
        }
    }

    if(modo == 1) //Separadores a negro para la mascara de colisiones y zonas de version y formato
    {
        for(i = 0; i < 8; i++)  //Separador horizontal sup izq
            setPixelAt(7, i, 1, bm);
        for(i = 0; i < 8; i++)  //Separador vertical sup izq
            setPixelAt(i, 7, 1, bm);
        for(i = 0; i < 8; i++)  //
            setPixelAt(bm->size - 8, i, 1, bm); //Separador horizontal inf izq
        for(i =  bm->size - 8; i <  bm->size ; i++)  //
            setPixelAt(i, 7, 1, bm); //Separador vertical inf izq
        for(i = bm->size - 8; i <  bm->size; i++)  //
            setPixelAt(7, i, 1, bm); //Separador horizontal sup dcha
        for(i =  0; i <  8 ; i++)  //
            setPixelAt(i, bm->size - 8, 1, bm); //Separador vertical sup dcha

        for(i = 0; i < 9; i++)  //Format info sup izq
            setPixelAt(8, i, 1, bm);
        for(i = 0; i < 9; i++)  //Format info sup izq
            setPixelAt(i, 8, 1, bm);
        for(i =  bm->size - 8; i <  bm->size ; i++)  //Format info inf izq
            setPixelAt(i, 8, 1, bm);
        for(i = bm->size - 8; i <  bm->size; i++)  //Format info sup dcha
            setPixelAt(8, i, 1, bm);



        if(version >= 7)
        {
            //Version info inf izq
            for(i = 0; i < 6; i++)
            {
                setPixelAt(bm->size - 9, i, 1, bm);
                setPixelAt(bm->size - 10, i, 1, bm);
                setPixelAt(bm->size - 11, i, 1, bm);
            }

            //Version info sup derecha
            for(i = 0; i < 6; i++)
            {
                setPixelAt(i, bm->size - 9, 1, bm);
                setPixelAt(i, bm->size - 10, 1, bm);
                setPixelAt(i, bm->size - 11, 1, bm);
            }
        }
    }
}

// *********************************************************************************
// static void colocar_alignment_patterns(struct BitmapQR *bm, int modo)
//
//Coloca los alignment patterns en el bitmap
//Modo = 0 => Coloca los alignment patterns
//Modo = 1 => Coloca todo negro en los alignment patterns, para la mascara de colisiones
//********************************************************************************** */
static void colocar_alignment_patterns(struct BitmapQR *bm, int modo)
{
    int i, j;
    uchar alignment_pattern[4] = {0xFC, 0x6B, 0x1F, 0x80};
    int num_patterns;
    int num_rows;
    int coordenadas[46][2];

    uint version;

    version = 1 + ((bm->size - 21) / 4);

    num_rows = coordenadas_alignment_patterns[version - 1][0];

    num_patterns = 0;
    for(i = 0; i < num_rows; i++)
        for(j = 0; j < num_rows; j++)
        {
            if (((i == 0)&&(j == 0)) || ((i == 0)&&(j == num_rows-1)) || ((i == num_rows-1)&&(j == 0)))
                continue;
            coordenadas[num_patterns][0] = coordenadas_alignment_patterns[version - 1][i+1];
            coordenadas[num_patterns][1] = coordenadas_alignment_patterns[version - 1][j+1];
            num_patterns++;
			ResetWatchDog();
        }


    for(i = 0; i < num_patterns; i++)
    {
    int m, n;
    int cuenta_bit = 0;
    int cuenta_byte = 0;
    uchar aux_byte;
    uchar aux_bit;
    int posX, posY;

    posX = coordenadas[i][0] - 2; //El -2 es porque las coordenadas son del centro, y quiero empezar por la esquina sup izq
    posY = coordenadas[i][1] - 2; //El -2 es porque las coordenadas son del centro, y quiero empezar por la esquina sup izq
    cuenta_bit = 0;
    cuenta_byte = 0;
    for(m = posX + 0; m < posX + 5; m++)
    {
        for(n = posY + 0; n < posY + 5; n++)
        {
            aux_byte = alignment_pattern[cuenta_byte];
            aux_bit = ((aux_byte & (0x80 >> cuenta_bit)) != 0) ? 1 : 0;
            if(modo == 0)
                setPixelAt(m, n, aux_bit, bm);
            else if (modo == 1)
                setPixelAt(m, n, 1, bm);
            cuenta_bit++;
            if(cuenta_bit == 8)
            {
                cuenta_bit = 0;
                cuenta_byte++;
            }
			ResetWatchDog();
        }
    }
    }
}

// *********************************************************************************
// static void colocar_timing_pattern(struct QRcode *qrcode)
//
//Coloca los timing patterns en el qrcode
//********************************************************************************* */
static void colocar_timing_pattern(struct BitmapQR *bm, int modo)
{
    int i;

    if(modo == 0)
    {
    //Horizontal
    for(i = 0; i < bm->size; i+= 2)
        setPixelAt(6, i, 1, bm);

    //Vertical
    for(i = 0; i < bm->size; i+= 2)
        setPixelAt(i, 6, 1, bm);
    }
    else if(modo == 1)
    {
        //Horizontal
    for(i = 0; i < bm->size; i+= 1)
        setPixelAt(6, i, 1, bm);

    //Vertical
    for(i = 0; i < bm->size; i+= 1)
        setPixelAt(i, 6, 1, bm);
    }

}

// *********************************************************************************
// static void QRcoloca_version_information(struct QRcode *qrcode)
//
//Coloca la informacion sobre la version en el bitmap
//********************************************************************************* */
static void QRcoloca_version_information(struct QRcode *qrcode)
{
    int x, y;
    int i;
    uchar pixel_value;
    uint ver_info;


    if(qrcode->version < 7)
        return;

    x = 0;
    y = qrcode->codigo.size - 11;
    ver_info = version_information[qrcode->version - 7];
    for(i = 0; i <= 17; i++)
    {
        pixel_value = 0x1 & ver_info;
        ver_info = ver_info >> 1;
        setPixelAt(x, y, pixel_value, &qrcode->codigo);
        setPixelAt(y, x, pixel_value, &qrcode->codigo);
        if( (i+1)%3 == 0)
        {
            x++;
            y = qrcode->codigo.size - 11;
        }
        else
        {
            y++;
        }
        ResetWatchDog();
    }
}

// *********************************************************************************
// static void siguiente_libre(struct BitmapQR *bm, int *x, int *y, int *ud)
//
// Funcion recursiva que calcula el siguiente pixel en el que se deben colocar datos
// bm: bitmap con la mascara de puntos que no se pueden utilizar
// x, y: coordenadas del último punto utilizado. Tb devuelve ahí el siguiente punto libre
// lr: posición del último punto probado (0 = left, 1 = right)
// ud: dirección de movimiento dentro del simbolo (0 = up, 1 = down)
//********************************************************************************* */
static void siguiente_libre(struct BitmapQR *bm, int *x, int *y, int *ud)
{
    do
    {
        /* Salto el timing pattern vertical */
        if(*x == 6)
            *x = 5;

        if(*x > 6) /* A la derecha del timing pattern vertical */
        {
            if(*ud == 0)
            {
                if (*x % 2 == 0)
                {
                    *x = *x - 1;
                }
                else
                {
                    *x = *x + 1;
                    *y = *y - 1;
                    if(*y < 0)
                    {
                        *y = *y + 1;
                        *x = *x - 2;
                        *ud = 1;
                    }
                }
            }
            else if(*ud == 1)
            {
                if (*x % 2 == 0)
                {
                    *x = *x - 1;
                }
                else
                {
                    *x = *x + 1;
                    *y = *y + 1;
                    if(*y > bm->size - 1)
                    {
                        *y = *y - 1;
                        *x = *x - 2;
                        *ud = 0;
                    }
                }
            }
        }
        else if(*x < 6) /* A la izquierda del timing pattern vertical */
        {
            if(*ud == 0)
            {
                if (*x % 2 == 1)
                {
                    *x = *x - 1;
                }
                else
                {
                    *x = *x + 1;
                    *y = *y - 1;
                    if(*y < 0)
                    {
                        *y = *y + 1;
                        *x = *x - 2;
                        *ud = 1;
                    }
                }
            }
            else if(*ud == 1)
            {
                if (*x % 2 == 1)
                {
                    *x = *x - 1;
                }
                else
                {
                    *x = *x + 1;
                    *y = *y + 1;
                    if(*y > bm->size - 1)
                    {
                        *y = *y - 1;
                        *x = *x - 2;
                        *ud = 0;
                    }
                }
            }
        }
    } while (getPixelAt(*x, *y, bm) == 1);
}

// *********************************************************************************
// static void aplica_mascara_000(struct BitmapQR *bm, struct BitmapQR *colisiones)
//
//********************************************************************************* */
static void aplica_mascara_000(struct BitmapQR *bm, struct BitmapQR *colisiones)
{
    int x, y;
    for(x = 0; x < bm->size; x++)
    {
        for(y = 0; y < bm->size; y++)
        {
            if ((x + y) % 2 == 0)
            {
                if(getPixelAt(x, y, colisiones) == 0)
                {
                    InviertePixelAt(x, y, bm);
                }
            }
        }
    }
}

// *********************************************************************************
// static void aplica_mascara_001(struct BitmapQR *bm, struct BitmapQR *colisiones)
//
//********************************************************************************* */
static void aplica_mascara_001(struct BitmapQR *bm, struct BitmapQR *colisiones)
{
    int x, y;
    for(x = 0; x < bm->size; x++)
    {
        for(y = 0; y < bm->size; y++)
        {
            if (y % 2 == 0)
            {
                if(getPixelAt(x, y, colisiones) == 0)
                {
                    InviertePixelAt(x, y, bm);
                }
            }
        }
    }
}

// *********************************************************************************
// static void aplica_mascara_010(struct BitmapQR *bm, struct BitmapQR *colisiones)
//
//********************************************************************************* */
static void aplica_mascara_010(struct BitmapQR *bm, struct BitmapQR *colisiones)
{
    int x, y;
    for(x = 0; x < bm->size; x++)
    {
        for(y = 0; y < bm->size; y++)
        {
            if (x % 3 == 0)
            {
                if(getPixelAt(x, y, colisiones) == 0)
                {
                    InviertePixelAt(x, y, bm);
                }
            }
        }
    }
}

// *********************************************************************************
// static void aplica_mascara_011(struct BitmapQR *bm, struct BitmapQR *colisiones)
//
//********************************************************************************* */
static void aplica_mascara_011(struct BitmapQR *bm, struct BitmapQR *colisiones)
{
    int x, y;
    for(x = 0; x < bm->size; x++)
    {
        for(y = 0; y < bm->size; y++)
        {
            if ((x + y) % 3 == 0)
            {
                if(getPixelAt(x, y, colisiones) == 0)
                {
                    InviertePixelAt(x, y, bm);
                }
            }
        }
    }
}

// *********************************************************************************
// static void aplica_mascara_100(struct BitmapQR *bm, struct BitmapQR *colisiones)
//
//********************************************************************************* */
static void aplica_mascara_100(struct BitmapQR *bm, struct BitmapQR *colisiones)
{
    int x, y;
    for(x = 0; x < bm->size; x++)
    {
        for(y = 0; y < bm->size; y++)
        {
            if (((y / 2) + (x / 3)) % 2 == 0)
            {
                if(getPixelAt(x, y, colisiones) == 0)
                {
                    InviertePixelAt(x, y, bm);
                }
            }
        }
    }
}

// *********************************************************************************
// static void aplica_mascara_101(struct BitmapQR *bm, struct BitmapQR *colisiones)
//
//********************************************************************************* */
static void aplica_mascara_101(struct BitmapQR *bm, struct BitmapQR *colisiones)
{
    int x, y;
    for(x = 0; x < bm->size; x++)
    {
        for(y = 0; y < bm->size; y++)
        {
            if ((x*y) % 2 + (x*y) % 3 == 0)
            {
                if(getPixelAt(x, y, colisiones) == 0)
                {
                    InviertePixelAt(x, y, bm);
                }
            }
        }
    }
}

// *********************************************************************************
// static void aplica_mascara_110(struct BitmapQR *bm, struct BitmapQR *colisiones)
//
//********************************************************************************* */
static void aplica_mascara_110(struct BitmapQR *bm, struct BitmapQR *colisiones)
{
    int x, y;
    for(x = 0; x < bm->size; x++)
    {
        for(y = 0; y < bm->size; y++)
        {
            if ((((x*y) % 2 + (x*y) % 3)) % 2 == 0)
            {
                if(getPixelAt(x, y, colisiones) == 0)
                {
                    InviertePixelAt(x, y, bm);
                }
            }
        }
    }
}

// *********************************************************************************
// static void aplica_mascara_111(struct BitmapQR *bm, struct BitmapQR *colisiones)
//
//********************************************************************************* */
static void aplica_mascara_111(struct BitmapQR *bm, struct BitmapQR *colisiones)
{
    int x, y;
    for(x = 0; x < bm->size; x++)
    {
        for(y = 0; y < bm->size; y++)
        {
            if ((((x*y) % 3 + (x + y) % 2)) % 2 == 0)
            {
                if(getPixelAt(x, y, colisiones) == 0)
                {
                    InviertePixelAt(x, y, bm);
                }
            }
        }
        ResetWatchDog();
    }
}

// *********************************************************************************
// static void QRcoloca_format_info(struct QRcode *qrcode)
//
//Coloca la informacion de formato en el bitmap
//********************************************************************************* */
static void QRcoloca_format_info(struct QRcode *qrcode)
{

int pos_format1[15][2] = { {0,8}, {1,8}, {2,8}, {3,8}, {4,8}, {5,8}, {7,8}, {8,8}, {8,7}, {8,5}, {8,4}, {8,3}, {8,2}, {8,1}, {8,0} };
int pos_format2[15][2] = { {8,20}, {8,19}, {8,18}, {8,17}, {8,16}, {8,15}, {8,14}, {13,8}, {14,8}, {15,8}, {16,8}, {17,8}, {18,8}, {19,8}, {20,8} };

uchar b[32][2] = {
                        {0x54, 0x12}, {0x51, 0x25}, {0x5E, 0x7C}, {0x5B, 0x4B}, {0x45, 0xF9},
                        {0x40, 0xCE}, {0x4F, 0x97}, {0x4A, 0xA0}, {0x77, 0xC4}, {0x72, 0xF3},
                        {0x7D, 0xAA}, {0x78, 0x9D}, {0x66, 0x2F}, {0x63, 0x18}, {0x6C, 0x41},
                        {0x69, 0x76}, {0x16, 0x89}, {0x13, 0xBE}, {0x1C, 0xE7}, {0x19, 0xD0},
                        {0x07, 0x62}, {0x02, 0x55}, {0x0D, 0x0C}, {0x08, 0x3B}, {0x35, 0x5F},
                        {0x30, 0x68}, {0x3F, 0x31}, {0x3A, 0x06}, {0x24, 0xB4}, {0x21, 0x83},
                        {0x2E, 0xDA}, {0x2B, 0xED}
                        };

    int i;

    //Punto fijo a negro
    setPixelAt(8, qrcode->codigo.size - 8, 1, &qrcode->codigo);

    //Ajusto los valores de pos_format2, que dependen del tamaño
    for(i = 0; i < 7; i++)
    {
        pos_format2[i][1] += (qrcode->version -1)*4;
    }
    for(i = 7; i < 15; i++)
    {
        pos_format2[i][0] += (qrcode->version - 1)*4;
    }
    ResetWatchDog();
    for(i = 6; i >= 0; i--)
    {
        uchar aux;
        aux = bitAtByte(b[qrcode->format_info][0], i);
        setPixelAt(pos_format1[14 - (i + 8)][0], pos_format1[14 - (i + 8)][1], aux, &qrcode->codigo);
        setPixelAt(pos_format2[14 - (i + 8)][0], pos_format2[14 - (i + 8)][1], aux, &qrcode->codigo);
    }
    ResetWatchDog();
    for(i = 7; i >= 0; i--)
    {
        uchar aux;
        aux = bitAtByte(b[qrcode->format_info][1], i);
        setPixelAt(pos_format1[14 - i][0], pos_format1[14 - i][1], aux, &qrcode->codigo);
        setPixelAt(pos_format2[14 - i][0], pos_format2[14 - i][1], aux, &qrcode->codigo);
    }
    ResetWatchDog();
}

//**********************************************************************************
// static unsigned char bitAtByte(unsigned char dato, unsigned char pos)
//
//Devuelve el valor del bit que se encuentra en la posicion pos
//Los bits dentro del byte estan numerados 7..0
//********************************************************************************* */
static uchar bitAtByte(uchar dato, uchar pos)
{
    uchar mask;
    mask = 0x01 << pos;

    return ( (dato & mask) != 0) ? 1 : 0;
}

// *********************************************************************************
// struct QRcode *obtenQRcode(void)
//
//********************************************************************************* */
struct QRcode *obtenQRcode(void)
{
	return &qrcod;
}

uchar cal_qr(void)
{
    uchar errorLevel = 0; /* Un 0 => L y un 1 => M, por ahora no se han implementado los demas */
    struct QRcode *qrcode = (struct QRcode *)obtenQRcode();
    char *bufParser = (char *)obtenBufParser();
    uint num_dcb = obtenNumDig();
    
    QRcalcula_codigo_optimo(qrcode,bufParser, num_dcb, errorLevel);
    versionLastQR = qrcode->version;
     
	if (qrcode->version > 0)
        return (((qrcode->version - 1) * 4) + 21);
    else
        return 0;
}

#endif 


#if 0
// *********************************************************************************
// static uchar ObtenQR_ECI(void)
//                                 
//********************************************************************************* */
static uchar	ObtenQR_ECI(void)
{
	switch (ObtenParametro(ID_PARAM_CODEPAGE))
	{
		case 874:
			/*THAI*/
			return 13;	
		case 1250:
			/*CENTRAL EUROPE*/
			return 21;
		case 1251:
		#if EXTENSION_TECLADO_KAZAJO
		case 12511:
		#endif
			/*CYRILIC*/
			return 22;	
		case 1253:
			/*GREEK*/
			return 9;					
		case 1254:
			/*TURKISH=LATIN ALPHABET NUMBER 5*/
			return 11;			
		case 1256:
			/*ARABIC*/
			return 24;
		case 1257:		
			/*BALTIC*/
			return 15;	
		case 1252:
			default:
			return 0x03;		
	}
}
#endif
