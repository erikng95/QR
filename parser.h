
extern	uchar	imastr[],sepstr[],sepstr1[],indstr;

void	Initparser						(void);
void 	ini_parser						(void);
void 	put_parser						(uchar c);
uchar 	get_ctr_parser					(void);
void 	comesc							(void);
void 	com_0							(void);
void 	com_1							(void);
void 	com_2							(void);
void 	com_3							(void);
void 	com_4							(void);
void 	com_5							(void);
void 	com_6							(void);
void 	com_7							(void);
void 	com_8							(void);
void 	com_9							(void);
void 	com_dp							(void);
void 	com_pc							(void);
void 	com_me							(void);
void 	com_ig							(void);
void 	com_rec							(void);
void 	com_int							(void);
void 	com_arr							(void);
void 	com_A							(void);
void 	com_B							(void);
void 	com_C							(void);
void 	com_D							(void);
void 	com_E							(void);
void 	com_F							(void);
  #if OFFSET_ETIQUETA
void 	com_G							(void);
  #endif
void 	com_H							(void);
void 	com_I							(void);
void 	com_J							(void);
void 	com_L							(void);
void 	com_N							(void);
void 	com_P							(void);
void 	com_Q							(void);
void 	com_R							(void);
void 	com_U							(void);
void 	com_V							(void);
void 	com_X							(void);
void 	com_Y							(void);
void 	com_Z							(void);
void 	com_ac							(void);
void 	com_ct							(void);
void 	com_cp							(void);
void 	com_a							(void);
void 	com_b							(void);
void 	com_c							(void);
void 	com_d							(void);
void 	com_e							(void);
void 	com_f							(void);
void 	com_h							(void);
void 	com_i							(void);
void 	com_j							(void);
void 	com_k							(void);
void 	com_n							(void);
void 	com_o							(void);
void 	com_p							(void);
void 	com_q							(void);
void 	com_t							(void);
void 	com_u							(void);
void 	com_w							(void);
void 	com_x							(void);
void 	com_y							(void);
void 	com_z							(void);
void 	com_la							(void);
void 	comnad							(void);
int 	rd_int_buffer_parser			(void);
int 	rd_int2_buffer_parser			(void);
int 	rd_int5_buffer_parser			(void);
int 	rd_int							(uchar n);
long 	rd_long							(uchar n);
uchar 	rd_char_buffer_parser			(void);
void 	rd_tipo_letra					(uchar tl);
void 	tratar_cr						(void);
void 	tratar_lf						(uchar c);
void 	get_lg							(void);
uchar 	calcula_check_codigo_barras		(uchar e);
void 	wri2d5							(uchar e);
void 	int2d5							(uchar e);
void 	wriean							(uchar e);
void 	wriint							(uchar e,uchar c);
void 	ajust_rot						(int n);
void 	wr_motvel						(uchar c);
time 	rd_timer_deb					(void);
void 	do_prn							(void);
void 	calrss							(void);
void 	calrss_1						(int n,uchar m);
void 	calrss_2						(int n,uchar m);
void 	wrirss							(void);
void 	calrss_chk						(void);
uchar 	dorss14							(uchar m,uchar n,uchar c,uchar modulo,uchar flg);
uchar 	getmask							(uchar m);
void 	getrsswidths					(int val,int n,int elements,int maxwidth,int nonarrow);
int 	combins							(int n,int r);
uchar 	incmodulo						(uchar modulo);
void 	wrirss_apilado					(void);
void 	wrirss_apilado_omni				(void);
void 	wrirss_expandido				(uchar c);
void 	rss_expanded_data				(char *s);
void 	PintaCodigoRSSExpandido			(void);
void 	CorreccionPosicionRotacion		(int Offset);
int 	alphanumeric_encode				(uchar *estado,char *s);
void 	calcula_databar_expanded		(void);
void 	anchos_valor_expanded			(uint valor,char *anchos);
void 	calcula_databar_expanded_stacked(void);
int 	hueco_barra						(int j);
int 	barra_hueco						(int j);
void 	cal_row							(void);
int 	calcula_codigo_barras_bruto		(void);
void 	calcula_separadores_bruto		(void);
int 	cuantos_numericos				(char *s);
uint 	ext_bitstr						(void);
void 	get_Rss14_anchura_elemento		(uchar *anch_elemento,int val,int n,int num_elemento,int anch_max,int noNarrow);
void 	ini_bitstr						(void);
void 	ins_bitstr						(uint data,uint nbits);
int 	iso_iec_encode					(uchar *estado,char *s);
int 	cuantos_alfanumericos			(char *s);
int 	cuantos_no_iso_iec				(char *s);
uchar 	es_alfanumerico					(uchar c);
void 	mostrar_bitstr					(void);
void 	mostrar_ingr					(void);
uchar 	no_es_alfanumerico				(uchar c);
int 	numeric_encode					(uchar *estado, char *s);
void 	pon_bitstr						(uchar pos,uchar c);
void 	wr_xmag							(int c);
int   	rd_xmag							(void);
void 	wr_ymag							(int c);
int   	rd_ymag							(void);
int 	rd_font							(void);
void 	wr_font							(int nFont);
uchar 	rd_rotacion						(void);
int 	rd_h_pos						(void);
int 	rd_v_pos						(void);
void 	wr_h_pos						(int x);
void 	wr_v_pos						(int x);
uchar 	rd_bytlin						(void);
int 	rd_CodepageImpresion			(void);
uchar 	rd_alt							(void);
uchar 	rd_densidad_ean					(void);

uchar 	rd_barras_stacked				(int i);
uchar 	rd_codigo_barras				(int i);
uchar 	rd_separadores					(int i);
void 	sepinf							(int k);
void 	sepmed							(int k);
void 	sepsup							(int k);
uchar 	rd_separadores_stacked			(int i);
void 	wr_barras_stacked				(int i,uchar c);
void 	wr_codigo_barras				(int i,uchar c);
void 	wr_separadores					(int i,uchar c);
void 	wr_separadores_stacked			(int i,uchar c);
void    ini_datamatrix                  (void);

uchar 	rd_flg_negritas					(void);

#define	LENPAR						1024
#define	MAX_COMANDOS_PARSER			78
#define	PAPEL_ETIQUETA				0 
#define	PAPEL_CONTINUO				1
#define	PAPEL_CONTINUO_ADHESIVO		2

#define	DOTCAB_2					448
#define DOTCAB_3					640

  #if DOT_12
#define	DISOPTO						642
#define DOTCAB_4					1280
#define	DOTS						1280
  #else
#define	DISOPTO						428
#define DOTCAB_4					832
#define	DOTS						832
  #endif

#define MAX_ANCH_CAB				DOTCAB_4/8
#define	BYTLINM						DOTS/8

/* Tipos de códigos de barras */
#define	EAN_13						0
#define	EAN_8						1
#define	UPC							2
#define	ITF_14						3
#define	ITF_6						4
#define	I_2D5						5
#define	U_3D9   					6
#define	E_128						7
#define	EAN_13_5					8
#define	RSS_14						9
#define	EAN_5						10
#define	EAN_2						11
#define E_DATAMAT					12
  #if CORR_EAN_COMO_LP_2
#define	E_EXPAND					13
  #endif

#define INICIO_A					16
#define INICIO_B 					17
#define INICIO_C 					18
#define CAMBIO_A 					19
#define CAMBIO_B 					20
#define CAMBIO_C 					21
#define CAMBIO__ 					22
#define FNC1 						23
#define STOP128 					24
#define IA_C						28						/* Parentesis de los identificadores */
#define IA_F						29

struct COORD_XY
{
   uchar	def;
   int 		x;
   int		y;
   uchar	r;
   int 		let;
   uchar	xmag;
   uchar	ymag;
   uchar	ena;
};
