
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
	unsigned char m[4100];
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
