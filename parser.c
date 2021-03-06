
//**************************************************************************************
// PARSER     							18-Nov-14                                      *
// Tarea de parser.                                                                    *
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
#include "tablas.h"
#include "SrvImpresion.h"
#include "SrvFonts.h"
#include "SrvMemoriaDinamica.h"
#include ".\FontsFlash\FontsFijos.h"
#include "Comms\ProtocoloDebug232.h"
#include "Assert.h"
#include "SrvFonts.h"
#include "SrvLogos.h"
  #if CORR_CAMBIO_FTO
#include "RegistrosImpresion.h"
#include ".\DrvHw\DrvFPGA.h"
  #endif


//**************************************************************************************
// VARIABLES                                                                           *
//**************************************************************************************
static	OS_TID	Id_parser;
static	char	buf_lin_logo[MAX_ANCH_CAB],ingr[2000],str[256];
static	uchar	buf_parser[LENPAR],flgesc,contraste_etiquetadora;
static	uchar	rotacion,flag_generar_cter_control,size_y;
static	uchar	densidad_ean;
  #if QR_CODE
uchar	buf_auxiliar_parser[1024],num_digitos_codigo_barras;
  #else
static	uchar	buf_auxiliar_parser[256],num_digitos_codigo_barras;
  #endif
static	uchar	posicion_linea_interpretacion,stacked,row_symbols;
static	uchar	datos_rss14,extrapadding,num_rows;
static	uchar	ORG_DATAMATRIX[144*18];
static	uchar	ORG_BARRAS_STACKED[1166],ORG_CODIGO_BARRAS[1166],ORG_SEPARADORES[1166];
static	uchar	ORG_SEP_STACKED[1166],bytlin;
static	int		indice_rd_parser,indice_wr_parser,num_copias,xpag,ypag;
static	int		h_pos,v_pos,horpos,verpos,font,alt,largo,ancho,grosor;
static	int		bits_in_row,CodepageImpresion,ind_bitstr,hposter,bits_in_last_row;
#if CORR_DATABAR_EXP_STACK
static	int 	xpag,ypag;	
#endif
static	int		xmag,ymag;
//static	long	valor_cont;
static	time	timer_deb;

static	uchar	rss_widths[8];

static	uchar	flg_negritas;

static	uchar	flg_datamatrix;

  #if QR_CODE
static  uchar   flg_qr;      /* variable que indica si el contenido de un EAN es un codigo QR en la parte de recepcion del mensaje "y" */
  #endif
  
static	uchar	flg3d9;
static	uchar	num128;				/* Numero de caracteres del EAN-128 */
static	uchar	ord128;				/* Numero de orden EAN-128 */
static	uchar	tipcod128;			/* Tipo de codigo */
static	long	sum128;				/* Suma EAN-128 */
static	uchar	flg128;				/* TRUE si componer 2� digito */
static	uchar 	inicio_letra;
static	uchar	flag_inicio_ean;
static	uchar	code128pr;

static	char ORG_CODEWORDS[114+48+14];		/*  176 bytes asignados a los codewords del Datamatrix */
static	char ORG_DAT_C[49*2+14];			/*  49 int + 14 bytes para una tabla del datamatrix */
static	uchar	num_datamatrix;

  #if OFFSET_ETIQUETA
static	long	off_izda,off_dcha,off_aba,off_arr;
  #endif

static	void 	ini_bitstr							(void);
static	void 	ins_bitstr							(uint data,uint nbits);
static	int 	numeric_encode						(uchar *estado,char *s);
static	int 	alphanumeric_encode					(uchar *estado,char *s);
static	void	rss_expanded_data					(char *s);
static	int 	cuantos_numericos					(char *s);
static	int 	iso_iec_encode						(uchar *estado,char *s);
static	void 	pon_bitstr							(uchar pos,uchar c);
static	void 	calcula_databar_expanded			(void);
static	void 	PintaCodigoRSSExpandido				(void);
static	void 	mostrar_ingr						(void);
static	void	calcula_databar_expanded_stacked	(void);
static	uchar	dib_datamatrix						(void);
static	uchar	cal_datamatrix						(void);
static	void	dat_datamatrix						(uchar n);
static	void	lin_datamatrix(uchar t);
static	void	dot_datamatrix(uchar col,uchar fil);
static	void	dup_datamatrix(uchar fil,uchar t,uchar n);
static	uchar	cal_error(uchar n);
static	void	redsolomon(uchar nd,uchar nc);
static	void	put_c(uchar i,int n);
static	int		get_c(uchar i);
static	int		prod(int x,int y,int gf);
static	void	com_datamatrix(uchar n,uchar i);
static	uchar	calpad(uchar i);
static	uchar	cnv_datamatrix(uchar c);

  #if OFFSET_ETIQUETA
static	int 	rd_offset(void);
  #endif

  #if DOT_12
static	uchar flg_nonum;
  #endif

  #if QR_CODE
static  void    dup_qr						(uchar fil, uchar t, uchar n, uchar *bm);
static  uchar 	dib_qr						(void);
  #endif

struct	COORD_XY	coord_xy[100];
uchar	tipo_campo;

//**************************************************************************************
// TABLAS                                                                              *
//**************************************************************************************
const	ptr_fun	tbl_comandos_parser[MAX_COMANDOS_PARSER] = {
	com_0,       /* '0'   */			/* Carga de logos             */
	com_1,       /* '1xx' */          	/* Velocidad impresora        */
	com_2,       /* '^2xxxxxx' */		/* Inicializa valor contador  */
	com_3,       /* '^3lll,gggg' */   	/* Linea en video inverso     */
	com_4,       /* '^4lll,ggg' */    	/* Linea en blanco            */
	com_5,       /* '5xxx' */           /* Nuevo Size Y               */
	com_6,       /* '6' */            	/* Linea fondo gris           */
	com_7,       /* '7' */              /* Zona de transporte         */
	com_8,       /* '8' */				/* Banco de IRAM (imagen)     */
	com_9,       /* '^9x' */          	/* Densidad EAN-13            */
	com_dp,      /* ':' */            	/* Recogedor papel            */
	com_pc,      /* ';' */            	/* Recogedor ribbon           */
	com_me,      /* '<' */            	/* Tipo de cabeza termica     */
	com_ig,      /* '=' */            	/* Carga de fonts de windows  */
	com_rec,     /* '>' */            	/* Motor recogida y ribbon    */
	com_int,     /* '?' */            	/* Forzar respuesta a la impr */
	com_arr,     /* '@x' */           	/* contraste etiquetadora     */
	com_A,       /* '^Atexto' */      	/* texto                      */
	com_B,       /* '^Bxxx,yyy,zzz' */	/* rectangulo                 */
	com_C,       /* '^Cxxx' */        	/* copias                     */
	com_D,       /* 'D' */          	/* 2 de 5                     */
	com_E,       /* '^Exxx,c' */      	/* altura barras y control    */
	com_F,       /* '^Fx,y' */        	/* tipo de letra 16x28        */
#if OFFSET_ETIQUETA
	com_G,       /* '^Gxxxx,yyyy' */    /* offset hor y vertical      */
#endif
	com_H,       /* '^Hxxx' */        	/* Pos. horizontal absoluta   */
	com_I,       /* '^Ix,y' */        	/* Tipo de letra 12x17.       */
	com_J,       /* '^Jx,y' */        	/* Tipo de letra 9x14.        */
	comnad,      /* '^Kxxxx...' */    	/* Codigo 39.                 */
	com_L,       /* '^Lxxx,yyy' */    	/* Dibujar linea.             */
	comnad,      /* 'M' */            	/*                            */
	com_N,       /* '^Nx' */          	/* Linea de interpretacion    */
	comnad,      /* '0???' */         	/* Nuevo origen coord.        */
	com_P,       /* '^P1234567' */    	/* Codigo barras EAN - 8.     */
	com_Q,       /* '^Q123456789012'*/	/* Codigo barras EAN - 13.    */
	com_R,       /* '^Rx' */          	/* Rotacion.                  */
	comnad,      /* 'S' */            	/* Contador.                  */
	comnad,      /* 'T' */            	/*                            */
	com_U,       /* '^U12345678901' */	/* Codigo de barras UPC.      */
	com_V,       /* '^Vxxxx' */       	/* Posicion vertical abs.     */
	comnad,      /* '^W'*/            	/* 1mm. horizontal +.         */
	com_X,       /* 'Xnnn' */         	/* Anchura del logo           */
	com_Y,       /* 'Ynnn' */         	/* Longitud del logo          */
	com_Z,       /* 'Zmddd....' */    	/* Datos del logo             */
	com_ac,      /* '[xcc' */           /* Campo variable             */
	comnad,      /* '\' */            	/*                            */
	com_ct,      /* '^]xxx,yyyy'*/    	/* Tama�o de pagina.          */
	comnad,      /* '^' */            	/*                            */
	comnad,      /* '_' */            	/*                            */
	com_cp,      /* '^`t'*/           	/* Tipo de papel              */
	com_a,       /* '^a' */           	/* Avance de papel.           */
	com_b,       /* '^bxxx' */        	/* Distancia salida dots.     */
	com_c,       /* '^c' */           	/* Borrar RAM imagen.         */
	com_d,       /* '^dx,y' */        	/* Tipo de letra 6x9.         */
	com_e,       /* '^ex,y' */        	/* Tipo de letra 16x32.       */
	com_f,       /* '^f' */           	/* Peso variable terminal     */
	comnad,      /* '^g' */           	/* Importe variable terminal  */
	com_h,       /* '^h' */           	/* Codigo de barras variable  */
	com_i,       /* '^ix' */          	/* Hab/deshab Modo terminal   */
	com_j,       /* '^jn' */          	/* Activar etiq o impr.       */
	com_k,       /* 'kx' */           	/* Contraste ticket           */
	comnad,      /* '^lxxx' */        	/* Avance de impresora.       */
	comnad,      /* 'm' */            	/*                            */
	com_n,       /* '^nx' */          	/* Emul impresora suprema.    */
	com_o,       /* '^oxxx' */        	/* Ajuste dist opto-cabeza    */
	com_p,       /* '^px,y' */        	/* Tipo de letra 12x17        */
	com_q,       /* 'q' */            	/* Cambio de codepage activo  */
	comnad,      /* 'r' */            	/* Ejecutar macro.            */
	comnad,      /* 's' */            	/* Definir macro.             */
	com_t,       /* 't' */            	/* ITF-14.                    */
	com_u,       /* 'u' */            	/* RSS-14 Databar             */
	comnad,      /* '^v' */           	/* Pedir estado papel.        */
	com_w,       /* '^wx' */          	/* Modo de encabezamiento     */
	com_x,       /* '^xnne' */        	/* Dibujar logo nn: e:efecto  */
	com_y,       /* '^ynnn...' */     	/* EAN 128                    */
	com_z,       /* 'z' */            	/* Font de Windows            */
	com_la,      /* '{' */            	/* Etiqueta en blanco         */
	comnad,      /* '|' */            	/*                            */
	comnad,      /* '}' */            	/*                            */
	};

//**************************************************************************************
// FUNCIONES INTERNAS                                                                  *
//**************************************************************************************
static 	__task 	void 	parser		(void);
static	void 	datcab		(int tipcab);

static void		int128			(uchar n);
static void		wri128			(uchar c);
static void		wri128e			(uchar c);
static void		chk128			(uchar c);
static uchar	tsttip			(uchar c);
static	void	wri3d9			(uchar c);



//**************************************************************************************
// INITPARSER 							18-Nov-14                                      *
// Inicializa la tarea de parser.                                                      *
//**************************************************************************************
void Initparser(void)
{
   Id_parser = InsertaTareaMonitor(parser);	
   Id_parser = Id_parser;
}

//**************************************************************************************
// PARSER      							18-Nov-14                                      *
// Tarea de parser.                                                                    *
//**************************************************************************************
static __task void parser(void)
{
   uchar cter;

   ini_parser();
   while (1)
     {
     if (flgesc == TRUE)				/* Leido Esc inesperado */
       {
       cter = 0x1b;
       flgesc = FALSE;             				
       }
     else
       cter = get_ctr_parser();  		/* Leer cter parser */

     if (cter == 0x1b)   				/* Cter es Escape */
       {
       cter = get_ctr_parser();
       if (cter == 0x1b)  				/* Es escape */
         comesc();                      /* Imprimir */                                         
       else
         {
         if (cter >= '0')
           cter -= '0';     			/* Offset comandos */
         else
           cter = MAX_COMANDOS_PARSER;
         if (cter < MAX_COMANDOS_PARSER);
           tbl_comandos_parser[cter]();	/* Ejecutar comando */
         }
       }
     Monitor();
     }
}

//**************************************************************************************
// INI_PARSER  							18-Nov-14                                      *
// Inicio de variables del parser.                                                     *
//**************************************************************************************
void ini_parser(void)
{
   indice_rd_parser = 0;
   indice_wr_parser = 0;
   flgesc = FALSE;
   memset(buf_parser,0x00,LENPAR);
   num_copias = 1;
   contraste_etiquetadora = 5;
//   tipo_papel = PAPEL_ETIQUETA;
   v_pos = 0;
   h_pos = 0;
#if CORR_DATABAR_EXP_STACK
   xpag = 448;
   ypag = 496;
#endif
   xmag = 1;
   ymag = 1;
   datcab(2);
   font = F_16X32;
   rotacion = 0;
//   flg_resp_prn = FALSE;
   CodepageImpresion = 0;
   size_y = 0;
}

//**************************************************************************************
// PUT_PARSER  							18-Nov-14                                      *
// Poner caracter en el buffer del parser.                                             *
//**************************************************************************************
void put_parser(uchar c)
{
   int i;

   i = ((indice_wr_parser+1)%LENPAR);
   if (i == indice_rd_parser)
   	 AssertNum(ASSERT_FULL_PARSER);
   while (i == indice_rd_parser)
     Monitor();
   buf_parser[indice_wr_parser] = c;
   indice_wr_parser = i;
}

//**************************************************************************************
// GET_CTR_PARSER                       18-Nov-14                                      *
// Leer cter del buffer del parser.                                                    *
//**************************************************************************************
uchar get_ctr_parser(void)
{
   uchar dato;
  
   if (flgesc == TRUE)
     return(0);
   while (indice_wr_parser == indice_rd_parser)
  {
     Monitor();
   }
   dato = buf_parser[indice_rd_parser++];
   indice_rd_parser = indice_rd_parser%LENPAR;
   return(dato);
}

//**************************************************************************************
// COMESC                               18-Nov-14                                      *
// Imprimir.                                                                           *
//**************************************************************************************
void comesc(void)
{
   int i;

   for (i=0;i<num_copias;i++)
     {
     do_prn();
     if (rd_flg_debug() == 'A')			// Debug
       LanzarTimer(timer_deb);			// Iniciar timer
  #if 0
     if (num_copias > 1)
       {
       n = inport(I_RD_ENTRADAS);		/* Para abortar la impresion de copias */
       if ((n & 0x0100) == 0x0000  ||  (n & 0x0200) == 0x0000)
         break;  
       getadc(OPTO2);                   /* Leer sensor de papel */
       if (bufadc[OPTO2] < nopapel)     /* No hay papel */
         break;
       }
     while (flags_impresion.imprimiendo == TRUE)	/* Esperar a fin de impresion */    
       monitor();    
     flags_impresion.imprimir = TRUE;
     ypag_prn = ypag;
     while (flags_impresion.imprimir == TRUE)		/* Esperar a inicio de impresion */	
	   monitor();    
     while (flags_impresion.imprimiendo == TRUE)	/* Esperar a fin de impresion */	       
	   monitor();
  #endif
     }
}

//**************************************************************************************
// COM_0                                19-Nov-14                                      *
// ^0xx... Carga de logos.                                                             *
// PENDIENTE DE PROBAR																   *
//**************************************************************************************
void com_0(void)
{	
	int Clave,nLogo,Sx,Sy,Orden,i;
	char pDatos[50];
	
	rd_char_buffer_parser();			/* Leer longitud */
	Clave = get_ctr_parser();			/* Leer clave */
	
	switch (Clave)
	{
		case '0':						/* Cabecera */
  #if AMPLIA_LOGOS_999
     	case '5':						/* Cabecera para los logos 999 */
  			nLogo = (uint) rd_int(3);
  #else
			nLogo = (uchar) rd_int(3);
  #endif		
			Sx = rd_int(5);
			Sy = rd_int(5);
  #if AMPLIA_LOGOS_999
       		if (Clave == '5')
       			nLogo = nLogo + ((uint) rd_int(1) * 1000);
  #endif
			GrabaCabeceraLogo485(nLogo, Sx, Sy);
			break;
		
		case '1':						/* Datos */
  #if AMPLIA_LOGOS_999
     	case '6':						/* Datos para los logos 999 */
  #endif
			Orden = rd_int(5);
			for ( i=0 ; i<50 ; i++)
				pDatos[i] = get_ctr_parser();
			GrabaDatosLogo485(Orden,pDatos);
       		break;
     }
}

//**************************************************************************************
// COM_1                                19-Nov-14                                      *
// ^1xx. Velocidad de impresion.                                                       *
//**************************************************************************************
void com_1(void)
{
#if !PRUEBA_FPGA
	//if (ObtenPasadaCalibracionAutomatica())
		SetVelocidadImpresionLS(rd_char_buffer_parser()); /* 00 - 99 */
#endif
}

//**************************************************************************************
// COM_2                                19-Nov-14                                      *
// ^2xxxxxx. valor del contador 1                                                      *
//**************************************************************************************
void com_2(void)
{
/*   valor_cont = */rd_long(6);
//   if (valor_cont != 0)					/* si el valor recibido es cero mantengo el valor que hubiera */
//     cont = valor_cont;					/* Pega: nunca se va a poder inicializar a cero */
}

//**************************************************************************************
// COM_3                                19-Nov-14                                      *
// ^3lll,gggg. Pintar una linea en video inverso.                                      *
//**************************************************************************************
void com_3(void)
{
   get_lg();							/* Leer largo y grosor */
   dibinv(largo,grosor);				/* Dibujar linea */
}

//**************************************************************************************
// COM_4                                19-Nov-14                                      *
// ^4lll,ggg. Pintar una linea en blanco.                        	                   *
//**************************************************************************************
void com_4(void)
{
   get_lg();							/* Leer largo y grosor */
   diblin(largo,grosor,0);              /* Dibujar linea */
}

//**************************************************************************************
// COM_5                                04-Mar-15                                      *
// ^5xxx. Nuevo Size Y.                                          	                   *
//**************************************************************************************
void com_5(void)
{
   size_y = (uchar) rd_int_buffer_parser();
}

//**************************************************************************************
// COM_6                                19-Nov-14                                      *
// ^6lll,gggg. Pintar una linea en fondo gris.                                         *
//**************************************************************************************
void com_6(void)
{
   get_lg();							/* Leer largo y grosor */
   dib_sombreado(largo,grosor,1);		/* Dibujar linea */
}

//**************************************************************************************
// COM_7                                19-Nov-14                                      *
// ^7iiii,ffff. Zona de transporte.                                                    *
//**************************************************************************************
void com_7(void)
{
//   ini_trans = */rd_int2_buffer_parser();	/* Leer inicio zona de transporte */
   get_ctr_parser();					/* Leer "," */
//   fin_trans = */rd_int2_buffer_parser();	/* Leer final zona de transporte */
//  flgtrans = TRUE;						/* Indicar que hay zona de transporte */
}

//**************************************************************************************
// COM_8                                19-Nov-14                                      *
// ^8i. Banco de imagen.                                                               *
//**************************************************************************************
void com_8(void)
{
   /*uchar c;*/

   /*c =*/ get_ctr_parser();
  #if 0
   bnkiram = c & 0x0f;
  #endif
}

//**************************************************************************************
// COM_9                                19-Nov-14                                      *
// ^9x. Densidad EAN-13.                                                               *
//**************************************************************************************
void com_9(void)
{
   uchar cter;

   cter = get_ctr_parser() & 0x0f;      /* Leer densidad */
   if (cter <= 9)
     densidad_ean = cter;
  #if DOT_12
   if (densidad_ean < 9)
     densidad_ean++;
   else
     densidad_ean = 0;					/* Densidad 9 ==> densidad 0 */
  #endif
}

//**************************************************************************************
// COM_DP                               18-Nov-14                                      *
// Activar recogedor papel soporte.                                                    *
//**************************************************************************************
void com_dp(void)
{
int actrec;

   actrec = get_ctr_parser() & 0x0f;             
   SetControlRecogedor(actrec);
}

//**************************************************************************************
// COM_PC                               18-Nov-14                                      *
// Activar recogedor ribbon.                                                           *
//**************************************************************************************
void com_pc(void)
{
int recrib;
	
   recrib = get_ctr_parser() & 0x0f;         
   SetControlRibbon(recrib);    
}

//**************************************************************************************
// COM_ME                               18-Nov-14                                      *
// Tama�o de cabeza termica.                                                           *
//**************************************************************************************
void com_me(void)
{
int tipcab;	
   tipcab = get_ctr_parser() & 0x0f;
   datcab(tipcab);
}

//**************************************************************************************
// COM_IG                               19-Nov-14                                      *
// Recibir los fonts de windows.                                                       *
//**************************************************************************************
void com_ig(void)
{
   uchar j;

   rd_char_buffer_parser();				/* Leer longitud */
   /*i = */(uchar) rd_int(3);				/* Numero del mensaje del sector */
   /*n = */rd_long(5);						/* Numero del sector */
   //bnksav = o_bancos;
   //SET_BNK(BNK_6);
   //pDest = (uchar *)ORG_TBLRX;
   for (j=0;j<16;j++)
     {
     /*a = */get_ctr_parser();
     /*b = */get_ctr_parser();
     //pDest[16*i+j] = hextob(a,b);
     }
   //if (i == 31)							/* Ultimo de un sector */
     //escribir_CF(_ORG_CF_FUENTES+n,0,pDest,TAM_SECTOR);
   //SET_BNK(bnksav);
}

//**************************************************************************************
// COM_REC                              19-Nov-14                                      *
// ^>xx. Activar motor de recogida o ribbon.                                           *
//**************************************************************************************
void com_rec(void)
{
   uchar m,c;

   m = get_ctr_parser() & 0x0f;         /* Leer motor a activar */
   c = get_ctr_parser() & 0x0f;         /* Leer activar o parar */
   switch (m)
     {
     case 0:							/* Motor de recogida */
       ActivarRecogedor(c);
       break;
     case 1:							/* Motor de ribbon */
       ActivarRibbon(m);
       break;
     }
}

//**************************************************************************************
// COM_INT                              19-Nov-14                                      *
// ^?x. Forzar contestacion.                                                           *
//**************************************************************************************
void com_int(void)
{
/*   uchar cter;*/

/*   cter = */(uchar) rd_int(2);              
 //  if (cter == 1)
 //    flg_resp_prn = TRUE;
}

//**************************************************************************************
// COM_ARR                              18-Nov-14                                      *
// ^@x. Contraste de etiquetadora 0 a 9.                                               *
//**************************************************************************************
void com_arr(void)
{
   contraste_etiquetadora = rd_char_buffer_parser();
   if (contraste_etiquetadora > 29)
     contraste_etiquetadora = 0;
   SetContraste (contraste_etiquetadora);
}

//**************************************************************************************
// COM_A                                19-Nov-14                                      *
// ^Axxx... Comando de texto.                                                          *
//**************************************************************************************
void com_A(void)
{
   uchar cter;
   uchar flg = FALSE;

   while (1)
     {		
     cter = get_ctr_parser();           /* Leer cter */
     if (cter == 0x1b)                  /* Fin del texto */
       {
       flgesc = TRUE;
       return;
       }
     if (cter == 0x0d)                  /* CR */
       {
       flg = TRUE;
       tratar_cr();
       continue;
       }
     if (cter == 0x0a)                  /* LF */
       {
       if (flg == FALSE)
         tratar_cr();
       flg = TRUE;
       tratar_lf(FALSE);
       Monitor();
       continue;
       }
     do_letter(cter);                   /* Hacer letra */

	if (cter == 0)
		break;
     flg = FALSE;
     }
     
}

//**************************************************************************************
// COM_B                                19-Nov-14                                      *
// ^Bxxx,yyy,zzz. Dibujar rectangulo.                                                  *
//**************************************************************************************
void com_B(void)
{
   int h;
	
  #if DOT_12
   largo = rd_int2_buffer_parser();    /* Leer largo */
   if (flg_nonum == FALSE)
     get_ctr_parser();                 /* Leer "," */
  #else
   largo = rd_int_buffer_parser();     /* Leer largo */
   get_ctr_parser();                   /* Leer "," */
  #endif
   ancho = rd_int2_buffer_parser();    /* Leer ancho */
   get_ctr_parser();                   /* Leer "," */
   grosor = rd_int_buffer_parser();    /* Leer grosor */
   if (grosor == 0  ||  largo < grosor  ||  ancho < grosor)
     return;
   h = h_pos;
   diblin(largo,grosor,1);         		/* Dibujar linea hor */
   diblin(grosor,ancho,1);              /* Dib. linea vertical */
   h_pos += largo-grosor;
   diblin(grosor,ancho,1);              /* Dib linea vertical */
   v_pos += ancho-grosor;
   h_pos = h;
   diblin(largo,grosor,1);              /* Dib. linea horizontal */
}

//**************************************************************************************
// COM_C                                19-Nov-14                                      *
// ^Cxxx. Numero de copias.                                                            *
//**************************************************************************************
void com_C(void)
{
   num_copias = rd_int_buffer_parser(); /* Leer numero de copias */
}

//**************************************************************************************
// COM_D                                19-Nov-14                                      *
// ^D??. 2D5.                                                                          *
//**************************************************************************************
void com_D(void)
{
   calcula_check_codigo_barras(I_2D5);  /* Calcular digito de control */
   wri2d5(I_2D5);                       /* Escribir 2D5 */
   int2d5(I_2D5);                       /* Escribir interpretacion */
}

//**************************************************************************************
// COM_E                                19-Nov-14                                      *
// ^Exxx,c. Altura y generacion digito control para codigo de barras.                  *
//**************************************************************************************
void com_E(void)
{
   alt = rd_int_buffer_parser();       	/* Leer altura */
  #if DOT_12
   alt = (alt * 12) / 8;
  #endif
   if (get_ctr_parser() == 0x1b)       	/* Leer "," */
     {
     flgesc = TRUE;
     return;
     }
   flag_generar_cter_control = TRUE;   	/* Caracter de control */
   if (get_ctr_parser() == 'D')
     flag_generar_cter_control = FALSE;
   tipo_campo = 2;
}

//**************************************************************************************
// COM_F                                19-Nov-14                                      *
// ^Fx,y. Tipo de letra 9x14.                                                          *
//**************************************************************************************
void com_F(void)
{
   rd_tipo_letra(F_9X14);             	/* Tipo de letra */
}

#if OFFSET_ETIQUETA
/************************************************************************/
/* COM_G                                18-Abr-16                       */
/* ^Gxxxx,yyyy offset hor y vert.                                       */
/************************************************************************/
void com_G(void)
{
   off_izda = rd_offset();     			/* Leer offset horizontal */
   off_dcha = 0;
   get_ctr_parser();					/* ',' */
   off_aba = rd_offset();     			/* Leer offset vertical */
   off_arr = 0;
}
#endif
//**************************************************************************************
// COM_H                                18-Nov-14                                      *
// ^Hxxx. Posicion horizontal.                                                         *
//**************************************************************************************
void com_H(void)
{
  #if DOT_12
   h_pos = rd_int2_buffer_parser();     /* Leer un entero de 4 bytes */
  #else
   h_pos = rd_int_buffer_parser();      /* Leer un entero de 3 bytes */
  #endif

#if CORR_DATABAR_EXP_STACK
   hposter = h_pos;
#endif

//   if (  == 1)					/* Centrado de etiqueta */
//     h_pos += (dotcab - xpag)/2;
//   else
//     h_pos += (dotcab - xpag);
   horpos = h_pos;
}

//**************************************************************************************
// COM_I                                19-Nov-14                                      *
// ^Ix,y. Tipo de letra 12x17.                                                         *
//**************************************************************************************
void com_I(void)
{
   rd_tipo_letra(F_12X17);              /* Tipo de letra */
}

//**************************************************************************************
// COM_J                                19-Nov-14                                      *
// ^Jx,y. Tipo de letra 16x28.                                                         *
//**************************************************************************************
void com_J(void)
{
   rd_tipo_letra(F_16X28);              /* Tipo de letra */
}

//**************************************************************************************
// COM_L                                19-Nov-14                                      *
// ^Lxxx,yyy. Dibujar linea.                                                           *
//**************************************************************************************
void com_L(void)
{
   get_lg();							/* Leer largo y grosor */
   if (grosor == 0)
     return;
   diblin(largo,grosor,1);                /* Dibujar linea */
}

//**************************************************************************************
// COM_N                                19-Nov-14                                      *
// ^Nx. Linea de interpretacion. (1-Abajo, 2-Encima, 3-Sin linea).                     *
//**************************************************************************************
void com_N(void)
{
   posicion_linea_interpretacion = get_ctr_parser() & 0x0f;
}

//**************************************************************************************
// COM_P                                19-Nov-14                                      *
// ^P1234567. EAN-8.                                                                   *
//**************************************************************************************
void com_P(void)
{
   if (calcula_check_codigo_barras(EAN_8) == FALSE)          
     return;
   wriean(EAN_8);                      /* Escribir EAN-8 */
   wriint(EAN_8,FALSE);                /* Escribir interpretacion */
}

//**************************************************************************************
// COM_Q                                19-Nov-14                                      *
// ^Q123456789012. EAN-13.                                                             *
//**************************************************************************************
void com_Q(void)
{
   switch (calcula_check_codigo_barras(EAN_13))	/* Calcular digito de control */
     {
     case 1:
       wriean(EAN_13);                  /* Escribir EAN */
       wriint(EAN_13,FALSE);            /* Escribir interpretacion */
       break;
     case 2:
       wriean(EAN_5);                  	/* Escribir EAN */
       wriint(EAN_5,FALSE);            	/* Escribir interpretacion */
       break;
     case 3:
       wriean(EAN_2);                  	/* Escribir EAN */
       wriint(EAN_2,FALSE);            	/* Escribir interpretacion */
       break;
     }
}

//**************************************************************************************
// COM_R                                18-Nov-14                                      *
// ^Rx. Rotacion.                                                                      *
//**************************************************************************************
void com_R(void)
{
   rotacion = get_ctr_parser() - '0';   /* Leer rotacion */
}

//**************************************************************************************
// COM_U                                20-Nov-14                                      *
// ^U12345678901. UPC.                                                                 *
//**************************************************************************************
void com_U(void)
{
   if (calcula_check_codigo_barras(UPC) == FALSE)            /* Calcular digito de control */
     return;
   wriean(UPC);                       	/* Escribir EAN-8 */
   wriint(UPC,FALSE);                   /* Escribir interpretacion */
}

//**************************************************************************************
// COM_V                                18-Nov-14                                      *
// ^Vxxxx. Posicion vertical absoluta.                                                 *
//**************************************************************************************
void com_V(void)
{
	v_pos = rd_int2_buffer_parser();
  #if OFFSET_ETIQUETA
   if (off_aba - off_arr != 0)
     v_pos += (int)(off_aba - off_arr);
   if (v_pos < 0)
     v_pos += ypag;
  #endif
	verpos = v_pos;
}

//**************************************************************************************
// COM_X                                18-Nov-14                                      *
// ^Xnnn. Anchura del logo.                                                            *
//**************************************************************************************
void com_X(void)
{
   /*x_logo = */rd_int_buffer_parser();    
}

//**************************************************************************************
// COM_Y                                11-Dic-14                       
// ^Ynnnn. Longitud del logo.	                                  	
//**************************************************************************************
void com_Y(void)
{
  /*y_logo = */rd_int2_buffer_parser();
  //ptrima = (char *) IMAGEN;
//  aux_h_pos = h_pos;
//  aux_v_pos = v_pos;
//  x = (uchar) (h_pos/8);
//  if (rotacion == 1)
//   x -= x_logo;
//  if (rotacion == 3)
//   x += x_logo;
//  y = v_pos;
//  bnk2 = FALSE;
//  aux_y = ((long) y * bytlin) + x;
//  if (aux_y >= 0x20000)
//   {
//     bnk2 = TRUE;
//     aux_y -= 0x20000;
//     y = (aux_y - x) / bytlin;
//   }
  //ptrima += ((long) y * bytlin) + x;	/* Apunta a la RAM imagen */
//  if (rotacion < 2)
//   ind_lin_logo = 0;
//  else
//   ind_lin_logo = x_logo;
}

void 	com_Z(void)
{
}

/************************************************************************/
/* COM_AC                               20-May-19                       */
/* ^[xcc. Coordenadas campos variables.                                 */
/************************************************************************/
void com_ac(void)
{
   uchar c,n;

   c = get_ctr_parser();
   if (c == 0x1b)
     {
     flgesc = TRUE;
     return;
     }
   n = rd_char_buffer_parser();
   if (n > 99)
     return;
   if (c == '0')						/* Coger los datos */
     {
     if (tipo_campo == 0)
       return;
     coord_xy[n].def = tipo_campo;
     coord_xy[n].x   = h_pos;
     coord_xy[n].y   = v_pos;
     coord_xy[n].r   = rotacion;
     if (tipo_campo == 1)				/* Font */
       {
       coord_xy[n].let  = font;
       coord_xy[n].xmag = xmag;
       coord_xy[n].ymag = ymag;
       coord_xy[n].ena  = flg_negritas;
       }
     else								/* EAN */
       {
       coord_xy[n].let = alt;
       coord_xy[n].ena = flag_generar_cter_control;
       }
     tipo_campo = 0;
     return;
     }
   if (coord_xy[n].def != 0)			/* Poner los datos */
     {
     h_pos = coord_xy[n].x;
     v_pos = coord_xy[n].y;
     rotacion = coord_xy[n].r;
     horpos = h_pos;
     verpos = v_pos;
     if (coord_xy[n].def == 1)			/* Font */
       {
       font = coord_xy[n].let;
       xmag = coord_xy[n].xmag;
       ymag = coord_xy[n].ymag;
       flg_negritas = coord_xy[n].ena & 0x01;
       }
     else								/* EAN */
       {
       alt = coord_xy[n].let;
       flag_generar_cter_control = coord_xy[n].ena;
       }
     }
}

//**************************************************************************************
// COM_CT                               18-Nov-14                                      *
// ^]xxx,yyyy. Tama�o pagina.                                                          *
//**************************************************************************************
void com_ct(void)
{
#if (!CORR_DATABAR_EXP_STACK)
int xpag,ypag;	
#endif
#if CORRECION_POSICION
static uchar PosicionamientoRealizado = FALSE;
#endif
#if CORR_CAMBIO_FTO
int yaux;
#endif

  #if DOT_12
   xpag = rd_int2_buffer_parser();		/* 4 caracteres */
   if (flg_nonum == FALSE)
	 get_ctr_parser();                 	/* Leer cter ',' */
  #else
   xpag = rd_int_buffer_parser();
//   if (xpag > dotcab)
//	 xpag = dotcab;
   get_ctr_parser();                   	/* Leer cter ',' */
  #endif
   ypag = rd_int2_buffer_parser();
   
#if OFFSET_ETIQUETA
   if (off_izda - off_dcha != 0)
     xpag += (int)(off_izda - off_dcha);
   if (off_aba - off_arr != 0)
     ypag += (int)(off_aba - off_arr);
#endif   

  #if CORR_CAMBIO_FTO
   if (PosicionamientoRealizado == TRUE)
   {
	 yaux = RdFpga(REG_LINEAS_ETI);
	 if (ypag < yaux)
		FuerzaDetencionImpresion();	
   }
  #endif

   SetDimensionEtiqueta(xpag,ypag);
#if CORRECION_POSICION	
   if (PosicionamientoRealizado == FALSE && ( ObtenEstadoOptoPapel() != NO_PAPEL ))
     {
	 // Hacer dos avances para posicionar 
     com_a();
     com_a(); 
     }
   PosicionamientoRealizado = TRUE;
#endif   
}

//**************************************************************************************
// COM_CP                               18-Nov-14                                      *
// ^`t. Tipo de papel (t=C continuo, t=E etiqueta).                                    *
//**************************************************************************************
void com_cp(void)
{
/*   uchar c;*/
	
//   tipo_papel = PAPEL_ETIQUETA;
/*   c = */get_ctr_parser();
//   if (c == 'C')
//     tipo_papel = PAPEL_CONTINUO;
//   if (c == 'A')
//     tipo_papel = PAPEL_CONTINUO_ADHESIVO;
}

//**************************************************************************************
// COM_A                                20-Nov-14                                      *
// ^a. Avance.                                                                         *
//**************************************************************************************
void com_a(void)
{
   while(Imprimiendo())
     Monitor();
   SetAvancePapel(TRUE);
   do_prn();
}

//**************************************************************************************
// COM_B                                18-Nov-14                                      *
// Distancia de salida.                                                                *
//**************************************************************************************
void 	com_b(void)
{
int dist_salida;

	dist_salida = (uchar) rd_int_buffer_parser();
#if !PRUEBA_FPGA
	SetDistanciaSalida(dist_salida);
#endif
}

//**************************************************************************************
// COM_C                                20-Nov-14                                      *
// ^c. Borrar RAM de imagen.                                                           *
//**************************************************************************************
void com_c(void)
{
   uchar i;
   clrima();                            /* Borrar RAM imagen */
   size_y = 0;
   for (i=0;i<100;i++)					/* Poner a 0 la estructura de las coordenadas */
     {
     coord_xy[i].def = FALSE;
     coord_xy[i].x = 0;
     coord_xy[i].y = 0;
     coord_xy[i].r = 0;
     coord_xy[i].let = 0;
     coord_xy[i].xmag = 0;
     coord_xy[i].ymag = 0;
     coord_xy[i].ena = FALSE;
     }
}

//**************************************************************************************
// COM_D                                18-Nov-14                                      *
// ^dx,y. Tipo de letra 6x9.                                                           *
//**************************************************************************************
void com_d(void)
{
   rd_tipo_letra(F_6X9);                /* Tipo de letra */
}

//**************************************************************************************
// COM_E                                18-Nov-14                                      *
// ^ex,y. Tipo de letra 16x32.                                                         *
//**************************************************************************************
void com_e(void)
{
   rd_tipo_letra(F_16X32);              /* Tipo de letra */
}

void 	com_f(void)
{
}

void 	com_h(void)
{
}

void 	com_i(void)
{
}

//**************************************************************************************
// COM_J                                18-Nov-14                                      *
// ^jn impresora o etiquetadora.                                                       *
//**************************************************************************************
void com_j(void)
{
   get_ctr_parser();
}

//**************************************************************************************
// COM_K                              20-Nov-14                                        *
// ^kx. Contraste de impresora de 0 a 9.                                               *
//**************************************************************************************
void com_k(void)
{
   get_ctr_parser() - '0';  			/* Contraste de 0 a 9 */
   //calton();
}

//**************************************************************************************
// COM_N                              20-Nov-14                                        *
// ^n. Modo emulacion.                                                                 *
//**************************************************************************************
void com_n(void)
{
}

//**************************************************************************************
// COM_O                                18-Nov-14                                      *
// ^oxxx. Ajuste de la distancia opto-cabeza.                                          *
//**************************************************************************************
void com_o(void)
{
   int n,dist_opto_cabeza;

   n = rd_int_buffer_parser();
   dist_opto_cabeza = DISOPTO;
  #if DOT_12
   dist_opto_cabeza += n;
  #else
   if (n <= 100)
     dist_opto_cabeza += n;
   else
     dist_opto_cabeza -= (n - 100);
  #endif
   SetDistanciaOptoCabeza(dist_opto_cabeza);
}

//**************************************************************************************
// COM_P                                20-Nov-14                                      *
// ^px,y. Tipo de letra 12x17.                                                         *
//**************************************************************************************
void com_p(void)
{
   rd_tipo_letra(F_12X17);
}

//**************************************************************************************
// COM_q                               			                                       *
// ^qx. Codepage a usar.                                                               *
//**************************************************************************************
void com_q(void)
{
   CodepageImpresion = rd_int2_buffer_parser();
}

//**************************************************************************************
// COM_T                                20-Nov-14                                      *
// ^t1234567890123. ITF-14.                                                            *
//**************************************************************************************
void com_t(void)
{
   if (calcula_check_codigo_barras(ITF_14) == FALSE)         /* Calcular digito de control */
     return;
   wri2d5(ITF_14);                      /* Escribir ITF-14 */
   int2d5(ITF_14);                      /* Escribir interpretacion */
}

//**************************************************************************************
// COM_U                                20-Nov-14                                      *
// ^ux1234567890123. RSS-14 Databar.                                                   *
//**************************************************************************************
void com_u(void)
{
   uchar i,j,c,d;
   
   j = 0;

   c = get_ctr_parser();				/* Tipo de RSS-14 */
#if CORR_RSS14
   if ((c >= INICIO_A  &&  c <= INICIO_C)  ||  (((c >= '0'  &&  c <= '9')  ||  (c >= 'A'  &&  c <= 'Z')) && buf_parser[indice_rd_parser] == FNC1))
#else   
   if ((c >= INICIO_A  &&  c <= INICIO_C)  ||  (c >= '0'  &&  c <= '9')  ||  (c >= 'A'  &&  c <= 'Z'))
#endif
     {
     if (c >= '0'  &&  c <= '9')		/* El primer dato es el numero de datos a poner en el RSS-14 */
       {
       datos_rss14 = c - '0';
       c = '7';
       }
     if (c >= 'A'  &&  c <= 'Z')		/* El primer dato es el numero de datos a poner en el RSS-14 */
       {
       datos_rss14 = c - 'A' + 10;		/* A=10, B=11, C=12 ... */
       c = '7';
       }
     if (c == INICIO_A)					/* Es un Databar Expandido */
       c = '4';							
     if (c == INICIO_B)					/* Es un Databar Expandido Apilado */
       c = '5';							
     if (c == INICIO_C)					/* Es un Databar Expandido Apilado */
       c = '6';							
     i = 0;
     j = 0;
     while (1)
       {
       d = get_ctr_parser();
       if (d == 0x1b)
         {
         flgesc = TRUE;
         break;
         }
       if (d >= INICIO_A  &&  d <= STOP128)
         continue;
       switch (d)
         {
         case IA_C:						/* Abrir parentesis */
           buf_lin_logo[j] = '(';
           break;
         case IA_F:						/* Cerrar parentesis */
           buf_lin_logo[j] = ')';
           break;
         default:
           buf_lin_logo[j] = d;
           break;
         }
       j++;
       if (d >= INICIO_A  &&  d <= IA_F)
         continue;						/* Saltar los caracteres especiales del EAN-128 */
       buf_auxiliar_parser[i++] = d;
       }
     buf_lin_logo[j] = 0x00;
     j = i;
     }
   else
     {
     buf_auxiliar_parser[0] = '0';
     buf_auxiliar_parser[1] = '1';
     for (i=0;i<14;i++)
       buf_auxiliar_parser[i+2] = get_ctr_parser();
     calrss();
     }
   i = FALSE;
   switch (c)
     {
     case '1':
       wrirss();						/* RSS-14 y RSS-14 truncado */
       break;
     case '2':
       i = TRUE;
       wrirss_apilado();				/* RSS-14 Apilado */
       break;
     case '3':
       i = 2;
       wrirss_apilado_omni();			/* RSS-14 Apilado Omnidireccional */
       break;
     case '4':							/* Databar Expandido */
     case '5':
     case '6':
     case '7':
       buf_auxiliar_parser[j] = 0x00;
       wrirss_expandido(c - '4');
       i = 3;
       break;
     }
   wriint(RSS_14,i);
}

//**************************************************************************************
// COM_W                                18-Nov-14                                      *
// ^wx. Cabecera. 0=Ret,no pr. 1=No ret,no pr. 2=Ret,pr. 3=No ret,pr.                  *
//**************************************************************************************
void com_w(void)
{
   uchar c;
	
   c = get_ctr_parser() - '0';         /* Leer modo de cabecera */
#if !PRUEBA_FPGA
   SetEncabezamiento(c);
#endif
}

/************************************************************************/
/* COM_X                                 4-Feb-2002                     */
/* ^xNNE. Logo.                                                         */
/*             N: N� de logo  (p.e. '01')                               */
/*             E: Efecto especial                                       */
/*	                 '0': no imprime                                    */
/*	                 '1': normal                                        */
/*	                 '2': inverso                                       */
/************************************************************************/
void com_x(void)
{
int nLogo,Efecto;
  #if AMPLIA_LOGOS_999
uchar logo_centena;
  #endif

/*uchar efecto,bnksav*/;
/*struct RECLOG  *ptrgraf;*/

	wr_xmag(1);
	wr_ymag(1);

	nLogo = (get_ctr_parser() - '0') * 10;
	nLogo += get_ctr_parser() - '0';
  #if AMPLIA_LOGOS_999
   logo_centena = get_ctr_parser();
   if (logo_centena == 0x1b)
     flgesc = TRUE;
   logo_centena -= '0';
   if (logo_centena <= 9)
     nLogo += (logo_centena * 100);
  #endif

   	Efecto = (get_ctr_parser() - '0');
   	
   	if (nLogo == 0)
   		return;
   		
   	if (Efecto == NO_PRN_LIN)
   		return;   	
   	
   	nLogo--;							// Trasladar a numero interno [0-9] Fijos, [10-177](impares son rotados)
   	if (EsLogoFijo(nLogo) == FALSE)
   	{	// Logo Prog, saltar los rotados
   		nLogo = (nLogo-MAX_LOGOS_FIJOS)*2+MAX_LOGOS_FIJOS;
   	}
	// Los logos programables van: los pares sin rotar y los impares rotados
	// Los rotados no los usamos, lo rota la fpga
	// Programar en la maquina, [1-10] Fijos, [11-94] Programables
	// Internamente [0-9] Fijos, [10-177](impares son rotados)
	
   	DibujaLogo( nLogo );
   	if (Efecto == EFECTO_INVERSO)
   		dibinv(ObtenSizeXLogo(nLogo), ObtenSizeYLogo(nLogo) );   	
   	
}

/************************************************************************/
/* COM_Y                                11-Ene-10                       */
/* ^ynnn... Imprimir codigo de barras EAN-128.                          */
/************************************************************************/
void 	com_y(void)
{
   uchar n,num;
   static uchar	cter;
   static int   h,v;
	
   num = 0;
   inicio_letra = FALSE;
   tipcod128 = 0;                       /* Reset tipo de codigo */
   sum128 = 0;               			/* Reset chk */
   ord128 = 1;                          /* Reset numero de orden */
   num128 = 0;                          /* Numero de caracteres */
   flg128 = FALSE;
   num_digitos_codigo_barras = 0;
   inistr();                            /* Reset buffer imagen */
   flag_inicio_ean = FALSE;
   n = 0;
   flg_datamatrix = FALSE;
#if QR_CODE
   flg_qr = FALSE;
#endif
   flg3d9 = FALSE;
   code128pr = FALSE;
   while (1)
     {
     cter = get_ctr_parser();           /* Leer cter */
     if (n == 1)						/* Para determinar si es EAN-128 o CODE 128 */
       {
       n = 2;
       if (cter == FNC1)
         code128pr = FALSE;
       else
         code128pr = TRUE;
       }
     if (n == 0)
       {
       if (cter == FNC1)				/* Comienza por FNC1 ==> es datamatrix */
         {
         flg_datamatrix = TRUE;
         buf_auxiliar_parser[num_digitos_codigo_barras++] = cter;
         cter = get_ctr_parser();    
#if QR_CODE
         if (cter == CAMBIO_C)  /* Comienza por FNC1 + CAMBIO_C==> es qr */
         {
             flg_qr = TRUE;
             flg_datamatrix = FALSE;
             cter = get_ctr_parser();
         }
#endif 
         n = 2;
         }
       else
         {
         if (cter != INICIO_A  &&  cter != INICIO_B  &&  cter != INICIO_C)
           {
           flg3d9 = TRUE;
           n = 2;
           wri3d9('*');
           }
         else
           n = 1;
         }
       }
     if (cter == 0x1b  ||  cter == STOP128)
       {
       if (flg_datamatrix == FALSE  &&  flg3d9 == FALSE
#if QR_CODE
           && flg_qr == FALSE
#endif
           )
         {
         sum128 %= 103;                 /* Calcular cter de check */
         tipcod128 = 2;                 /* Escribir cter control en tipo C */
//         SET_BNK_ROM(BNKROM_0);
         imaean((uchar)(tbl128[sum128]>>8),8);
//         SET_BNK_ROM(BNKROM_0);
         imaean((uchar)tbl128[sum128],3);
         num128++;
         imaean(0xc7,8);                /* 1� parte de parada */
         imaean(0x58,8);                /* 2� parte de parada */
         if (rotacion == 2)
           indstr++;
         num128++;
         }
       if (flg3d9 == TRUE)
         wri3d9('*');
       h = h_pos;
       v = v_pos;
       if (flg_datamatrix == TRUE)
         num = dib_datamatrix();
#if QR_CODE
       else if (flg_qr == TRUE)
           num = dib_qr();
#endif
       else
         dibean(8,E_128);             	/* Dibujar EAN-128 */
       h_pos = h;
       v_pos = v;
       if ((
#if QR_CODE           
           flg_qr == FALSE && 
#endif           
           flg_datamatrix == FALSE)  ||  num != 0  ||  flg3d9 == TRUE)
         int128(num);                   /* Linea de interpretacion */
       if (cter == 0x1b)
         flgesc = TRUE;
       break;
       }
     if (flg_datamatrix == FALSE  &&  flg3d9 == FALSE 
#if QR_CODE
        && flg_qr == FALSE
#endif
         )
       {
       if (tsttip(cter) == TRUE)        /* Cter especial */
         wri128e(cter);
       else
         wri128(cter);                  /* Cter normal */
       }
     if (flg3d9 == TRUE)
       wri3d9(cter);
     buf_auxiliar_parser[num_digitos_codigo_barras++] = cter;
     }
}

//**************************************************************************************
// COM_z                                11-Dic-14                       
// ^znnnn... Cambio a tipo de letra telecargado ampliado                
//**************************************************************************************
void com_z(void)
{
   uint	nFont;
   uchar Mag;

	/************************************************/
	/* Leer cuatro caracteres del numero de font    */
	nFont = rd_int2_buffer_parser();
	Mag = nFont %10;
	nFont = nFont - Mag;

	xmag = TablaMagnificacionFontTelec[Mag][0];
	ymag = TablaMagnificacionFontTelec[Mag][1];
    flg_negritas = FALSE;
    if (Mag == 8  ||  Mag == 9)
      flg_negritas = TRUE;

	if (CargaFont(nFont) >= 0)
	{
		font = nFont;
	}
	else
	{
		font = F_12X17;
		CodepageImpresion = CODEPAGE;
	}

}

//**************************************************************************************
// COM_LA                               20-Nov-14                                      *
// ^{ Imprimir la etiqueta en blanco.                                                  *
//**************************************************************************************
void com_la(void)
{

}

//**************************************************************************************
// COMNAD                               20-Nov-14                                      *
// Nada.                                                                               *
//**************************************************************************************
void comnad(void)
{
}

//**************************************************************************************
//* RD_INT_BUFFER_PARSER                 18-Nov-14                                     *
//* Leer un entero de 3 bytes del buffer del parser.                                   *
//**************************************************************************************
int rd_int_buffer_parser(void)
{
   return(rd_int(3));
}

//**************************************************************************************
// RD_INT2_BUFFER_PARSER                 18-Nov-14                                     *
// Leer un entero de 4 bytes del buffer del parser.                                    *
//**************************************************************************************
int rd_int2_buffer_parser(void)
{
   return(rd_int(4));
}

//**************************************************************************************
// RD_INT5_BUFFER_PARSER                18-Nov-14                                      *
// Leer un entero de 5 bytes del buffer del parser.                                    *
//**************************************************************************************
int rd_int5_buffer_parser(void)
{
   return(rd_int(5));
}

//**************************************************************************************
// RD_INT                               18-Nov-14                                      *
// Leer un entero.                                                                     *
//**************************************************************************************
int rd_int(uchar n)
{
   return((int) rd_long(n));
}

//**************************************************************************************
// RD_LONG                               18-Nov-14                                     *
// Leer un long.                                                                       *
//**************************************************************************************
long rd_long(uchar n)
{
   static long  x;
   static uchar i;
   uchar  c;
	
  #if DOT_12
   flg_nonum = FALSE;
  #endif
   if (flgesc == TRUE)
     return(0);
   x = 0;
   for (i=0;i<n;i++)
     {
     c = get_ctr_parser();
     if (c == 0x1b)
       {
       flgesc = TRUE;
       break;
       }
  #if DOT_12
     if (c < '0'  ||  c > '9')
       {
       flg_nonum = TRUE;
       break;
       }
  #endif
     x = 10 * x + (long)(c-'0');
     }
   return(x);
}

//**************************************************************************************
// RD_CHAR_BUFFER_PARSER                18-Nov-14                                      *
// Leer un uchar de 2 bytes del buffer del parser.                                     *
//**************************************************************************************
uchar rd_char_buffer_parser(void)
{
   return((uchar) rd_int(2));
}

//**************************************************************************************
// DATCAB                               18-Nov-14                                      *
// Datos de las cabezas.                                                               *
//**************************************************************************************
static	void datcab(int tipcab)
{
int dotcab;
	
   switch (tipcab)
     {
     case 0:                            /* 2" */
       dotcab = DOTCAB_2;
       break;
     case 1:                            /* 3" */
       dotcab = DOTCAB_3;
       break;
     default:                           /* 4" */
       dotcab = DOTCAB_4;
       break;
     }
   bytlin = (uchar) (dotcab / 8);
   //bytl = bytlin;

   //wordlin = bytlin / 2;
   SetNumeroDotsCabeza(dotcab);
}

//**************************************************************************************
// RD_TIPO_LETRA                        18-Nov-14                                      *
// Lee el tipo de letra del buffer del parser.                                         *
//**************************************************************************************
void rd_tipo_letra(uchar tl)
{
   uchar c;
	
   font = tl;                      		/* Tipo de letra */
   c = get_ctr_parser();                /* Leer X mag */
   c -= '0';
  #if FONT_8
   if (c > 3  &&  font != F_16X32  &&  c != 8)
  #else
   if (c > 3)
  #endif
     c = 3;
   xmag = c;
	c = get_ctr_parser();               /* Esperar ',' */
    flg_negritas = FALSE;
    if (c == '.')						/* Negritas */
      flg_negritas = TRUE;

   c = get_ctr_parser();                /* Leer Y mag */
   c -= '0';
   ymag = c;
   tipo_campo = 1;
}

//**************************************************************************************
// TRATAR_CR                            19-Nov-14                                      *
// Tratar CR.                                                                          *
//**************************************************************************************
void tratar_cr(void)
{
   if (rotacion == 0  ||  rotacion == 2)/* Rot 0� o 180� */
     h_pos = horpos;
   else
     v_pos = verpos;
}

//**************************************************************************************
// TRATAR_LF                            19-Nov-14                                      *
// Tratar LF.                                                                          *
//**************************************************************************************
void tratar_lf(uchar c)
{
int n;

	if (EsFontFijo(rd_font()) == FALSE)	
	{
		if (size_y != 0)
			n = size_y;
		else
			n = (ObtenSizeYFontNF(rd_font()) + ObtenEspaciadoYFontNF(rd_font())) * ymag;
        
		switch (rotacion)
		{	
		case 0:                            /* rotacion = 0, 0� */
			verpos += n;
			v_pos = verpos;
			break;
		case 1:                            /* rotacion = 1, 90� */
			horpos -= n;
			h_pos = horpos;
			break;
		case 2:                            /* rotacion = 2,180� */
			verpos -= n;
			v_pos = verpos;
			break;
		case 3:                            /* rotacion = 3,270� */
			horpos += n;
			h_pos = horpos;
			break;
		}
	}
	else
	{
		if (size_y != 0)
			n = size_y;
		else
			n = ObtenAltoTotalFontFijo(font) * ymag;
		
		switch (rotacion)
		{	
		case 0:                            /* rotacion = 0, 0� */
            if (c == TRUE)
			  v_pos += n;
            else
              {
			  verpos += n;
			  v_pos = verpos;
              }
			break;
		case 1:                            /* rotacion = 1, 90� */
            if (c == TRUE)
			  h_pos -= n;
            else
              {
			  horpos -= n;
			  h_pos = horpos;
              }
			break;
		case 2:                            /* rotacion = 2,180� */
            if (c == TRUE)
			  v_pos -= n;
            else
              {
			  verpos -= n;
			  v_pos = verpos;
              }
			break;
		case 3:                            /* rotacion = 3,270� */
            if (c == TRUE)
			  h_pos += n;
            else
              {
			  horpos += n;
			  h_pos = horpos;
              }
			break;
		}
	}
}

//**************************************************************************************
// GET_LG                               19-Nov-14                                      *
// Leer largo y grosor.                                                                *
//**************************************************************************************
void get_lg(void)
{
  #if DOT_12
   largo = rd_int2_buffer_parser();		/* Leer largo de la linea */
   if (flg_nonum == FALSE)
     get_ctr_parser();					/* Leer "," */
  #else
   largo = rd_int_buffer_parser();		/* Leer largo de la linea */
   get_ctr_parser();					/* Leer "," */
  #endif
   grosor = rd_int2_buffer_parser();	/* Leer grosor de la linea */
}

//**************************************************************************************
// CALCULA_CHECK_CODIGO_BARRAS          19-Nov-14                                      *
// Calcular digito de control.                                                         *
//**************************************************************************************
uchar calcula_check_codigo_barras(uchar e)
{
   static uchar i,suma,p,n,chk,j;
   uchar  c;
	
   chk = TRUE;                          /* Calculo de check */
   switch (e)
     {
     case EAN_13:
       n = 13;
       p = 1;
       break;
     case EAN_8:
       n = 8;
       p = 3;
       break;
     case UPC:
       n = 12;
       p = 3;
       break;
     case ITF_14:
     case RSS_14:
       n = 14;
       p = 3;
       chk = TRUE;
       break;
     case ITF_6:
       n = 6;
       p = 3;
       chk = TRUE;
       break;
     case I_2D5:
       n = 100;
       p = 3;
       chk = flag_generar_cter_control;
       break;
     }
   suma = 0;                           	/* Reset suma */
   for (i=0;i<n-1;i++)
     {
     c = get_ctr_parser();           	/* Leer digito */
     if (c == 0x1b)                  	/* Fin si es Esc no esperado */
       {
       flgesc = TRUE;
       if (e != I_2D5)             		/* Codigos con numero variable */
         {
         if (i == 5)					/* Es un EAN-5 */
           return(2);
         if (i == 2)					/* Es un EAN-2 */
           return(3);
         return(FALSE);
         }
       num_digitos_codigo_barras = i;
       if (flag_generar_cter_control == TRUE)
         num_digitos_codigo_barras++;
       if ((num_digitos_codigo_barras % 2) != 0)           /* Numero impar, a�adir un cero */
         {
         for (j=num_digitos_codigo_barras;j!=0;j--)
           buf_auxiliar_parser[j] = buf_auxiliar_parser[j-1];
         buf_auxiliar_parser[0] = 0;
         num_digitos_codigo_barras++;
         i++;
         }
       break;
       }
     c &= 0x0f;
     buf_auxiliar_parser[i] = c;     	/* Escribir cter en el buffer */
     suma += p * c;
     if (p == 1)                     	/* Peso del cter */
       p = 3;
     else
       p = 1;
     }
   if (chk == FALSE)                   	/* Sin cter de control */
     return(TRUE);
   suma %= 10;
   if (suma != 0)
     suma = 10 - suma;
   buf_auxiliar_parser[i] = suma;
   return(TRUE);
}

//**************************************************************************************
// WRI2D5                               14-Nov-14                                      *
// Escribir 2 De 5 entrelazado.                                                        *
//**************************************************************************************
void wri2d5(uchar e)
{
   static uchar i,j,b,s,k;
   static int   h,c;

   //SET_BNK (BANCO_IRAM);
   h = h_pos;                           /* Salvar h_pos */
   //nbit = (uchar) (h_pos%8);
   inistr();                            /* Init buffer de imagen */
   imaean(0x50,5);                      /* Separador lateral izq. */
   switch (e)
     {
     case ITF_14:                       /* ITF-14 */
       j = 14;                          /* 7 pares de digitos */
       break;
     case ITF_6:                        /* ITF-6 */
       j = 6;                           /* 3 pares de digitos */
       break;
     case I_2D5:                        /* 2D5 */
       j = num_digitos_codigo_barras;
       break;
     }
   for (i=0;i<j;i++)
     {
     b = tbl2d5[buf_auxiliar_parser[i++] & 0x0f];    /* Barras */
     s = tbl2d5[buf_auxiliar_parser[i] & 0x0f];      /* Espacios */
     c = 0x0000;
     for (k=0;k<5;k++)                  /* 5 bits por cter */
       {
       if (b & 0x80)                    /* Barra ancha */
         {
         c = c << 2;
         c |= 0x0003;
         }
       else                             /* Barra estrecha */
         {
         c = c << 1;
         c |= 0x0001;
         }
       if (s & 0x80)                    /* Espacio ancho */
         c = c << 2;
       else                             /* Espacio estrecho */
         c = c << 1;
       b = b << 1;
       s = s << 1;
       }
     c = c << 2;
     imaean((uchar)(c>>8),8);           /* Escribir parte alta */
     imaean((uchar)c,6);                /* Escribir parte baja */
     //tstmon();
     }
   imaean(0xd0,8);                      /* Separador lateral dcho. */
   dibean(8,e);                         /* Puntos mas largos */
   h_pos = h;                           /* Recuperar h_pos */
   //SET_BNK (0);
}

//**************************************************************************************
// INT2D5                               19-Nov-14                                      *
// Escribir linea de interpretacion 2 de 5.                                            *
//**************************************************************************************
void int2d5(uchar e)
{
   uchar i;
   uchar j = 0;

   if (posicion_linea_interpretacion == 3)
     return;
   font = F_12X17;
   xmag = 1;
   ymag = 1;
  #if DOT_12
   ymag = 2;
  #endif
   if (rotacion == 0)
     {
     if (posicion_linea_interpretacion == 2)
       v_pos -= 22;
     else
       v_pos += alt + 6;
     }
   if (rotacion == 1)
     {
     if (posicion_linea_interpretacion == 2)
       h_pos += 7;
     else
       h_pos -= alt + 6;
     }
   if (rotacion == 2)
     {
     if (posicion_linea_interpretacion == 2)
       v_pos += 22;
     else
       v_pos -= alt + 6;
     }
   if (rotacion == 3)
     {
     if (posicion_linea_interpretacion == 2)
       h_pos -= 22;
     else
       h_pos += alt + 4;
     }
   switch (e)
     {
     case ITF_14:                      	/* ITF-14 */
       if (rotacion == 0)
         h_pos += 10;      
       if (rotacion == 1)
         v_pos += 8;        	
       if (rotacion == 2)
         h_pos -= 10;
       if (rotacion == 3)
         /* v_pos += 14 *15; */
         v_pos -= 8;        	
       j = 14;
       break;
     case ITF_6:                       	/* ITF-6 */
       if (rotacion == 0)
         h_pos += 10;
       if (rotacion == 1)
         /* v_pos -= (14 * 7); */            	/* Modulos * Xmag */
         v_pos += 8;        	
       if (rotacion == 2)
         h_pos -= 2;
       if (rotacion == 3)
         /* v_pos -= 16 * 2; */
         v_pos -= 8;        	
       j = 6;
       break;
     case I_2D5:                       	/* 2D5 */
       if (rotacion == 0)
         h_pos += 10;
       if (rotacion == 1)
         /* v_pos -= (14 * (num_digitos_codigo_barras+1)); */
         v_pos += 8;        	
       if (rotacion == 2)
         h_pos -= 2;
       if (rotacion == 3)
         /* v_pos += (num_digitos_codigo_barras+1) * 15; */
         v_pos -= 8;        	
       j = num_digitos_codigo_barras;
       break;
     }
   for (i=0;i<j;i++)
     do_letter(buf_auxiliar_parser[i] | 0x30);
}

//**************************************************************************************
// WRIEAN                               19-Nov-14                                      *
// Escribir EAN.                                                                       *
//**************************************************************************************
void wriean(uchar e)
{
   static uchar i,c,m,l;
   static int   h,v;
   static uchar j = 0;
   static uchar k = 0;
	
   //SET_BNK (BANCO_IRAM);
   h = h_pos;                          	/* Salvar h_pos */
   v = v_pos;                          	/* Salvar v_pos */   
   //nbit = (uchar) (h_pos%8);
   inistr();                           	/* Init buffer de imagen */
   //tstmon();
   switch (e)
     {
#if IMPRIMIR_EAN_2
     case EAN_2:
#endif
     case EAN_5:
       imaean(0x58,5);
       break;
#if (!IMPRIMIR_EAN_2)
     case EAN_2:
       break;
#endif
     default:
       imasep(0x0a,7,TRUE);             /* Separador lateral izq. */
       break;
     }
   //tstmon();
   switch (e)
     {
     case EAN_13:                       /* EAN-13 */
       m = buf_auxiliar_parser[0] & 0x0f;	/* EAN-13, cambia juego A y B */
       m = tbl_juego[m];
       j = 1;
       k = 6;
       l = 13;
       break;
     case EAN_8:                      	/* EAN-8, siempre juego A */
       m = 0;
       j = 0;
       k = 3;
       l = 8;
       break;
     case UPC:                        	/* UPC, siempre juego A */
       m = 0;
       j = 0;
       k = 5;
       l = 12;
       break;
     case EAN_5:						/* EAN-5 */
       m = 0;
       j = 3;
       for (i=0;i<5;i++)
         {
         m += j * (buf_auxiliar_parser[i] & 0x0f);
         if (j == 3)
           j = 9;
         else
           j = 3;
         }
       m %= 10;
       //SET_BNK_ROM(BNKROM_0);
       m = tbl_juego_ean5[m];
       j = 0;
       k = 4;
       l = 0;
       break;
     case EAN_2:						/* EAN-2 */
#if IMPRIMIR_EAN_2
	   m = buf_auxiliar_parser[0] & 0x0f;
       m *= 10;
       m += buf_auxiliar_parser[1] & 0x0f;
       m %= 4;
       m = tbl_juego_ean2[m];
       j = 0;
       k = 1;
       l = 0;
#endif
       break;
     }
   for (i=j;i<=k;i++)                   /* Parte izda */
     {
     c = buf_auxiliar_parser[i] & 0x0f; /* Leer cter del EAN */
     if (m & 0x80)                    	/* Mirar juego de repr */
       c = tbl_ean_b[c];
     else
       c = tbl_ean_a[c];
     m = m << 1;                        /* Rotar mascara */
     imaean(c,7);
#if IMPRIMIR_EAN_2
     if ((e == EAN_5  ||  e == EAN_2)  &&  i < k)
#else
     if (e == EAN_5  &&  i < k)
#endif
       imaean(0x40,2);
     //tstmon();
     }
   if (e != EAN_5  &&  e != EAN_2)
     {
     imasep(0x50,5,FALSE);              /* Separador central */
     //tstmon();
     for (i=k+1;i<l;i++)                /* Parte dcha */
       {
       c = buf_auxiliar_parser[i] & 0x0f;/* Leer cter del EAN */
       c = ~tbl_ean_a[c];               /* Juego C = !juego A */
       imaean(c,7);
       //tstmon();
       }
     imasep(0xa0,7,TRUE);               /* Separador lateral dcho. */
     //tstmon();
     }
#if IMPRIMIR_EAN_2
   if ((e == EAN_5  ||  e == EAN_2)  &&  rotacion != 0)
#else
   if (e == EAN_5  &&  rotacion != 0)
#endif
     indstr++;
   dibean(8,e);                         /* Puntos mas largos */
   h_pos = h;                           /* Recuperar h_pos */
   v_pos = v;                          	/* Recuperar v_pos */   
   //SET_BNK (0);						/* Restaura el banco por defecto */
}

//**************************************************************************************
// WRIINT                               19-Nov-14                                      *
// Escribir linea de interpretacion.                                                   *
//**************************************************************************************
void wriint(uchar e,uchar c)
{
	uchar i,j,k,n,m;
    uchar num;
	k = 0;
	j = 0;
	
	font = F_12X17;
	xmag = 1;
	ymag = 1;
  #if DOT_12
    ymag = 2;
  #endif
	n = 2 + densidad_ean;

	if (rotacion == 0)
		v_pos += alt + 6;
	if (rotacion == 1)
#if CORR_RSS14
	  {
#endif	
		h_pos -= alt + 22;
		if (h_pos < 0)
			h_pos = 0;
#if CORR_RSS14
	  }	
#endif
	if (rotacion == 2)
#if CORR_RSS14
	  {
#endif	
		v_pos -= alt + 6;
		if (v_pos < 0)
			v_pos = 0;
#if CORR_RSS14
	  }	
#endif
	if (rotacion == 3)
		  h_pos += alt + 4;
	m = 8 + 4 * densidad_ean;
	switch (e)
		{
		case EAN_13:                     	/* EAN-13 */
			if (rotacion == 0)
  #if CORR_EAN_13 && !CORR_EAN_13_2
				h_pos += 4;
  #else
				h_pos -= 8;
  #endif	
			if (rotacion == 1)
				{
  #if CORR_EAN_13 && !CORR_EAN_13_2
					h_pos += 21;
  #else		
					h_pos += 17;
					v_pos -= 6;
  #endif
  #if 0
					if (densidad_ean == 0)
						v_pos -= (95 * n) + 22 ;     /* Modulos * Xmag */
					else
						v_pos -= (95 * n) + 31 ;     /* Modulos * Xmag */
  #endif
				}
			if (rotacion == 2)
                {
  #if !CORR_EAN_13 || CORR_EAN_13_2
				h_pos += 6;
  #endif
  #if 0
                if (densidad_ean != 0)
                  h_pos += 8 * densidad_ean;
  #endif
                }
			if (rotacion == 3)
			  {
  #if CORR_EAN_13 && !CORR_EAN_13_2
				v_pos += 4;
  #else
				v_pos += 6;
  #endif		
			  }	
			j = 13;
			k = 6;
			break;
		case EAN_8:                      	/* EAN-8 */
			m = 8;
			if (densidad_ean == 1)
				m = 16;
			if (rotacion == 0)
				{
				h_pos += 16;
				if (densidad_ean == 1)
				  h_pos += 12;
				}
			if (rotacion == 1)
                {
				h_pos += 17;
				v_pos += 17;
				/* v_pos -= (67 * n) - 2; */        	/* Modulos * Xmag */
				}	
			if (rotacion == 2)
				h_pos -= 16;
			if (rotacion == 3)
  				v_pos -= 17;
			j = 8;
			k = 3;
			break;
		case UPC:                          /* UPC */
			if (rotacion == 0)
				{
				h_pos += 16;
				if (densidad_ean == 1)
				h_pos += 8;
				}
			if (rotacion == 1)
				{
				h_pos += 17;
				v_pos += 17;
				/* v_pos -= (95 * n) - 6; */        	/* Modulos * Xmag */
			    }
			if (rotacion == 2)
				{
				h_pos -= 16;
  #if 0
				if (densidad_ean == 1)
				h_pos -= 8;
  #endif
				}
			if (rotacion == 3)
				{
				v_pos -= 17;
  #if 0
				if (densidad_ean == 1)
					{
					v_pos -= 4;
					m += 4;
					}
  #endif
				}
			j = 12;
			k = 5;
			break;
		case RSS_14:				/* RSS_14-13 */
          if (densidad_ean == 2  ||  c == TRUE  ||  c == 2  ||  c == 3)
	        font = F_6X9;
          if (c == 3)
            ymag = 2;
          if (rotacion == 0)
            {
            if (c != 3)
              h_pos -= 16;
            if (c == TRUE)
              v_pos -= 10;
            }
		  if (rotacion == 1)
  		    {
            if (c != 3)
			  v_pos -= 17;
            if (c == FALSE)
			  h_pos += 17;
            if (c == TRUE)
              h_pos -= alt - 20 - 20;
            if (c == 2)
              h_pos -= alt - 5;
            if (c == 3)
              {
#if !CORR_EAN_ROTADO_90
              if (stacked == 0)
                h_pos += alt + 17;
              else
#endif     
                h_pos += 17;
              }
			}
		  if (rotacion == 2)
            {
            if (c == 3)
              h_pos = horpos;
            else
              {
              if (densidad_ean == 0)
		        h_pos += 10;
              if (densidad_ean == 1)
                {
                if (c == 2)
                  h_pos += 110;
                else
                  h_pos += 190;
                }
              if (densidad_ean == 2)
                h_pos -= 80;
              if (c == TRUE)
                v_pos += 10;
              }
            }
          if (rotacion == 3)
            {
            if (c == FALSE)
              {
              if (densidad_ean == 0)
			    v_pos += 13 * 16;
              if (densidad_ean == 1)
                v_pos += 400;
              if (densidad_ean == 2)
                v_pos += 110;
              }
            if (c == TRUE)
              {
              v_pos += 115;
              h_pos += 15;
              }
            if (c == 2)
              {
              v_pos += 115;
              h_pos += alt + 12;
              if (densidad_ean == 1)
                v_pos += 100;
              }
            if (c == 3)
              {
#if !CORR_EAN_ROTADO_90
              if (stacked == 0)
                h_pos -= alt;
#endif
              v_pos = verpos;
              }
            }
          if (c == 3)
            j = strlen(buf_lin_logo);
          else
            j = 16;
          break;	
        case EAN_5:
          if (rotacion == 0)
            h_pos += 16;
          if (rotacion == 1)
            {
			h_pos += 17;
			v_pos += 16;
			}	
          if (rotacion == 2)
            {
#if CORR_EAN_5
 			h_pos -= 10;
#else
            h_pos -= 40;
            if (densidad_ean != 0)
              h_pos -= 20;
#endif     
            }
          if (rotacion == 3)
            {
#if CORR_EAN_5
            v_pos -= 16;
#else
            v_pos += 6*14;
            if (densidad_ean != 0)
              v_pos += (6*14)/2;
#endif     
            }
          j = 5;
          k = 5;
          break;
#if IMPRIMIR_EAN_2
        case EAN_2:
          if (rotacion == 0)
            h_pos += 8;
          if (rotacion == 1)
            {
			h_pos += 17;
			v_pos += 8;
			}	
          if (rotacion == 2)
            h_pos -= 10;
          if (rotacion == 3)
            v_pos -= 8;

          j = 2;
          k = 2;
          break;
  #endif
		}
    num = 0;
	for (i=0;i<j;i++)
		{
        if (e == RSS_14  &&  c == 3)
          {
		  do_letter(buf_lin_logo[i]);
          if (stacked == TRUE)			/* Expandido apilado */
            {
            num++;
            if (num * 8 > bits_in_row * 2  &&  j - i > 3)	/* Ajustar linea de int al ancho del codigo */
              {							/* Si faltan mas de 5 caracteres */
              tratar_cr();
              tratar_lf(TRUE);
              num = 0;
              }
            }
          }
        else
		  do_letter(buf_auxiliar_parser[i] | 0x30);
        if (e == RSS_14)
          {
          if (densidad_ean == 1)
            {
            if (c == TRUE  ||  c == 2)
              ajust_rot(6);
            else
              ajust_rot(4);
            }
          continue;
          }
		if (e == EAN_13  &&  i == 0)       /* Separacion 1er cter */
			{
			n = 10 + 10 * densidad_ean;
            ajust_rot(n);
			continue;
			}
		if (i == k)                        /* Separacion central */
			{
			n = m;
            ajust_rot(n);
			continue;
			}
        ajust_rot(8 * densidad_ean);
		}
}

//**************************************************************************************
// AJUST_ROT                 			19-Nov-14                                      *
// Ajustar posicion con la rotacion.                                                   *
//**************************************************************************************
void ajust_rot(int n)
{
   if (rotacion == 0)
     h_pos += n;
   if (rotacion == 1)
     v_pos += n;
   if (rotacion == 2)
     h_pos -= n;
   if (rotacion == 3)
     v_pos -= n;
}

//**************************************************************************************
// RD_TIMER_DEB              			20-Nov-14                                      *
// Leer timer_deb.                                                                     *
//**************************************************************************************
time rd_timer_deb(void)
{
   return(timer_deb);
}

//**************************************************************************************
// DO_PRN                    			20-Nov-14                                      *
// Imprimir.                                                                           *
//**************************************************************************************
void do_prn(void)
{
	while ( Imprimir() != FALSE )
		Monitor();	// Esperando que termine la impresion anterior

	while ( PermisoComandosImpresion() == FALSE )
		Monitor();	// No lanzar nuevos comandos a la Fpga hasta que termine de hacer el copiado de bufferes

}

/************************************************************************/
/* DIB_DATAMATRIX                       20-Ene-10                       */
/* Dibujar codigo Datamatrix.                                           */
/************************************************************************/
uchar dib_datamatrix(void)
{
   uchar i,n,tam;
   char aux[4];

  #if DOT_12
   alt = ((alt * 8) + 6) / 12;
  #endif
   for (i=0;i<MAXALTEAN;i++)			/* Con la altura del EAN calcular la densidad */
     {
     memmove(aux,&tblalt[i][2],3);
     aux[3] = 0x00;
     if (alt <= (uchar) atoi(aux))
       break;
     }
   tam = i;
  #if DOT_12
   tam = ((tam * 12) + 4) / 8;
  #endif
   ini_datamatrix();
   n = cal_datamatrix();
   if (n == 0)
     return(0);
#if CORR_DATAMATRIX_2
   indstr = ((n/8) * (tam + 3)) + 1;      
#else
   indstr = ((n/8) + 1) * (tam + 3);      
#endif
   for (i=0;i<n;i++)
     {
     if (i % 2 == 0)
       dot_datamatrix(i,0);				/* Dibujar primera linea de la L pattern */
     }
   for (i=1;i<n-1;i++)					/* Dibujar la L pattern */
     {
     dot_datamatrix(0,i);
     if (i % 2 != 0)
       dot_datamatrix(n-1,i);
     }
   for (i=0;i<n;i++)
     dot_datamatrix(i,n-1);				/* Linea final */
   dat_datamatrix(n-2);   				/* Dibujar los datos del datamatrix */
   for (i=0;i<n;i++)
     {
     dup_datamatrix(i,tam,n);
     lin_datamatrix(tam);				/* Dibujar cada linea del datamatrix */
     }
   return(n);
}

/************************************************************************/
/* CAL_DATAMATRIX                       20-Ene-10                       */
/* Calcular numero de columnas del Datamatrix.                          */
/************************************************************************/
uchar cal_datamatrix(void)
{
   uchar i,n,c,f,d;
   char	*ptr;
   
   d = 0;
   ptr = (char *)ORG_CODEWORDS;
   n = 0;
   f = FALSE;
   for (i=0;i<num_digitos_codigo_barras;i++)
     {
     c = buf_auxiliar_parser[i];
     if (c == IA_C  || c == IA_F)		/* Los parentesis, nada */
       continue;
     if (c >= '0'  &&  c <= '9')		/* Numerico, va en parejas */
       {
       if (f == FALSE)					/* Es el primero de la pareja */
         {
         f = TRUE;
         d = c;
         continue;
         }
       n++;								/* Es el segundo de la pareja */
       f = FALSE;
       d = (10 * (d - '0')) + (c - '0') + 130;			/* Forma de codificar las parejas de numeros */
       *ptr++ = d;
       continue;
       }
     if (f == TRUE)						/* Quedo una pareja incompleta */
       {
       f = FALSE;
       n++;
       d = d + 1;
       *ptr++ = d;						/* Forma de codificar los ASCII */
       }
     n++;								/* No es numerico */
     switch (c)
       {
       case CAMBIO_A:
         d = 0x1e;
         break;
       case FNC1:
         d = 232;						/* Codificar el FNC1 */
         if (buf_auxiliar_parser[i+1] == CAMBIO_A)
           {
           d = 0x1e;
           i++;
           }
         break;
       default:
         if (c > 0x80)					/* A�adir caracter Upper Shift */
           {
           *ptr++ = 235;				/* A�adir el caracter */
           n++;
           c = cnv_datamatrix(c);		/* Convertir el caracter de codepage de dos al de windows y en lower */
           }
         d = c + 1;						/* Forma de codificar los ASCII */
         break;
       }
  #if 0
     if (c == FNC1)
       d = 232;							/* Codificar el FNC1 */
     else
       {
       if (c > 0x80)					/* A�adir caracter Upper Shift */
         {
         *ptr++ = 235;					/* A�adir el caracter */
         n++;
         c = cnv_datamatrix(c);			/* Convertir el caracter de codepage de dos al de windows y en lower */
         }
       d = c + 1;						/* Forma de codificar los ASCII */
       }
  #endif
     *ptr++ = d;						
     }
   if (f == TRUE)						/* Quedo una pareja incompleta */
     {
     n++;
     d = d + 1;
     *ptr++ = d;						/* Forma de codificar los ASCII */
     }
   i = cal_error(n);					/* Codewords de error */
   if (i == 0)
     return(0);
   redsolomon(num_datamatrix,i);
   num_datamatrix = num_datamatrix + i;	/* Datos totales */

   if (num_datamatrix == 8)
     return(10);
   if (num_datamatrix == 12)
     return(12);
   if (num_datamatrix == 18)
     return(14);
   if (num_datamatrix == 24)
     return(16);
   if (num_datamatrix == 32)
     return(18);
   if (num_datamatrix == 40)
     return(20);
   if (num_datamatrix == 50)
     return(22);
   if (num_datamatrix == 60)
     return(24);
   if (num_datamatrix == 72)
     return(26);
   if (num_datamatrix == 98)
     return(32);
   if (num_datamatrix == 128)
     return(36);
  #if 0
   if (num_datamatrix == 162)
     return(40);
  #endif
   return(0);
}

/************************************************************************/
/* DAT_DATAMATRIX                       20-Ene-10                       */
/* Dibujar los datos del datamatrix.                                    */
/************************************************************************/
void dat_datamatrix(uchar n)
{
   uchar fil,col,dato,bit,c;
   char	*ptr;
   bit = 0;
   dato = 0;
   ptr = (char *)ORG_CODEWORDS;

   for (fil=0;fil<n;fil++)
     {
     for (col=0;col<n;col++)
       {
       switch (num_datamatrix)
         {
         case 8:
           dato = tblecc_8[fil][col][0];/* Dato a poner en cada posicion de la matriz */
           bit = tblecc_8[fil][col][1];
           break;
         case 12:
           dato = tblecc_10[fil][col][0]; 	
           bit = tblecc_10[fil][col][1];
           break;
         case 18:
           dato = tblecc_12[fil][col][0];
           bit = tblecc_12[fil][col][1];
           break;
         case 24:
           dato = tblecc_14[fil][col][0];
           bit = tblecc_14[fil][col][1];
           break;
         case 32:
           dato = tblecc_16[fil][col][0];
           bit = tblecc_16[fil][col][1];
           break;
         case 40:
           dato = tblecc_18[fil][col][0];
           bit = tblecc_18[fil][col][1];
           break;
         case 50:
           dato = tblecc_20[fil][col][0];
           bit = tblecc_20[fil][col][1];
           break;
         case 60:
           dato = tblecc_22[fil][col][0];
           bit = tblecc_22[fil][col][1];
           break;
         case 72:
           dato = tblecc_24[fil][col][0];
           bit = tblecc_24[fil][col][1];
           break;
         case 98:
           dato = tblecc_28[fil][col][0];
           bit = tblecc_28[fil][col][1];
           break;
         case 128:
           dato = tblecc_32[fil][col][0];
           bit = tblecc_32[fil][col][1];
           break;
  #if 0
         case 162:
           dato = tblecc_36[fil][col][0];
           bit = tblecc_36[fil][col][1];
           break;
  #endif
         }

       if (dato == 0)					/* Dato fijo */
         {
         if (bit == 1)
           dot_datamatrix(col+1,fil+1);	/* Si esta a 1 poner el dot */
         }
       else
         {
         c = *(ptr+dato-1);				/* Dato */
         if (c & tblbit[bit-1])
           dot_datamatrix(col+1,fil+1);	/* Si esta a 1 poner el dot */
         }
       }
     }
}

/************************************************************************/
/* LIN_DATAMATRIX                       14-Ene-10                       */
/* Dibujar cada linea del datamatrix.                                   */
/************************************************************************/
void lin_datamatrix(uchar t)
{
   alt = t + 3;
   dibean(8,E_DATAMAT);                		/* Dibujar EAN-128 */
   if (rotacion == 0)
     v_pos += alt;
   if (rotacion == 1)
#if CORR_DATAMATRIX
	 h_pos -= alt;
#else
     h_pos -= 0;
#endif
   if (rotacion == 2)
     v_pos -= alt;
   if (rotacion == 3)
#if CORR_DATAMATRIX
	 h_pos += alt;
#else
     h_pos += 0;
#endif
}

/************************************************************************/
/* INI_DATAMATRIX                       18-Ene-10                       */
/* Pone a cero los dots del datamatrix.                                 */
/************************************************************************/
void ini_datamatrix(void)
{
   uchar f,c;
   char	*ptr;

   ptr = (char *)ORG_DATAMATRIX;
   for (f=0;f<144;f++)					/* 144 filas */
     {
     for (c=0;c<18;c++)					/* 144 / 8 columnas */
       *ptr++ = 0x00;
     }
}

/************************************************************************/
/* DOT_DATAMATRIX                       18-Ene-10                       */
/* Dibujar cada dot del datamatrix.                                     */
/************************************************************************/
void dot_datamatrix(uchar col,uchar fil)
{
   uchar b,c;
   char	*ptr;

   ptr = (char *)ORG_DATAMATRIX;
   ptr += (fil * 18) + (col / 8);
   b = tblbit[col % 8];					/* bit a escribir */
   c = *ptr;
   c &= ~b;
   c |= b;
   *ptr = c;
}

/************************************************************************/
/* DUP_DATAMATRIX                       18-Ene-10                       */
/* Pasar de la imagen al string del datamatrix.                         */
/************************************************************************/
void dup_datamatrix(uchar fil,uchar t,uchar n)
{
   uchar a,i,c,m,j,k,d,mj,jj;
   char	*ptr;

   memset(imastr,0x00,200);
   a = t + 3;

   ptr = (char *)ORG_DATAMATRIX;
   ptr += fil * 18;
   jj = 0;
   mj = 0x80;
   n = (n/8) + 1;
   for (i=0;i<n;i++)
     {
     m = 0x80;							/* Mascara */
     d = *ptr++;						/* Dato leido */
     for (j=0;j<8;j++)
       {
       c = d & m;						/* bit leido */
       m = m >> 1;
       for (k=0;k<a;k++)				/* Repetir el bit n veces */
         {
         if (c != 0x00  &&  jj < 200)
           imastr[jj] |= mj;
         mj = mj >> 1;
         if (mj == 0x00)
           {
           mj = 0x80;
           jj++;
           }
         }
       }
     }
}

/************************************************************************/
/* CAL_ERROR                            12-Abr-11                       */
/* Calcular numero de codewords de error.                               */
/************************************************************************/
uchar cal_error(uchar n)
{
   if (n <= 3)
     {
     com_datamatrix(n,3);
     return(5);
     }
   if (n <= 5)
     {
     com_datamatrix(n,5);
     return(7);		
     }
   if (n <= 8)
     {
     com_datamatrix(n,8);
     return(10);	
     }
   if (n <= 12)
     {
     com_datamatrix(n,12);
     return(12);	
     }
   if (n <= 18)
     {
     com_datamatrix(n,18);
     return(14);	
     }
   if (n <= 22)
     {
     com_datamatrix(n,22);
     return(18);	
     }
   if (n <= 30)
     {
     com_datamatrix(n,30);
     return(20);	
     }
   if (n <= 36)
     {
     com_datamatrix(n,36);
     return(24);	
     }
   if (n <= 44)
     {
     com_datamatrix(n,44);
     return(28);	
     }
   if (n <= 62)
     {
     com_datamatrix(n,62);
     return(36);	
     }
   if (n <= 86)
     {
     com_datamatrix(n,86);
     return(42);	
     }
  #if 0
   if (n <= 114)
     {
     com_datamatrix(n,114);
     return(48);	
     }
  #endif
   return(0);
}

/************************************************************************/
/* REDSOLOMON                           22-Ene-10                       */
/* Calcular los codewords de error.                                     */
/************************************************************************/
void redsolomon(uchar nd,uchar nc)
{
   int i,j,k;

   char	*ptr;

   for (i=1;i<=nc;i++)
     put_c(i,0);
   put_c(0,1);

   for (i=1;i<=nc;i++)
     {
     put_c(i,get_c(i-1));
     for (j=i-1;j>=1;j--)
       put_c(j,get_c(j-1) ^ prod(get_c(j),alog[i],256));
     put_c(0,prod(get_c(0),alog[i],256));
     }

   ptr = (char *)ORG_CODEWORDS;
   ptr += nd;
   for (i=nd;i<=(nd+nc);i++)
     *ptr++ = 0;
   ptr = (char *)ORG_CODEWORDS;
   for (i=0;i<nd;i++)
     {
     k = *(ptr+nd) ^ *(ptr+i);
     for (j=0;j<nc;j++)
       *(ptr+nd+j) = *(ptr+nd+j+1) ^ prod(k,get_c(nc-j-1),256);
     }
}

/************************************************************************/
/* PUT_C                                22-Ene-10                       */
/* Poner dato en la tabla C.                                            */
/************************************************************************/
void put_c(uchar i,int n)
{
   char	*ptr;

   ptr = (char *)ORG_DAT_C;
   ptr += 2 * i;						/* Son int */
   *ptr = n;
}

/************************************************************************/
/* GET_C                                22-Ene-10                       */
/* Leer dato de la tabla C.                                             */
/************************************************************************/
int get_c(uchar i)
{
   int  n;
   char	*ptr;

   ptr = (char *)ORG_DAT_C;
   ptr += 2 * i;						/* Son int */
   n = *ptr;
   return(n);
}

/************************************************************************/
/* PROD                                 20-Ene-10                       */
/* Calcular los codewords de error.                                     */
/************************************************************************/
int	prod(int x,int y,int gf)
{
   if (!x || !y)
     return(0);
   else
     return(alog[(log[x] + log[y]) % (gf - 1)]);
}

/************************************************************************/
/* COM_DATAMATRIX                       20-Ene-10                       */
/* Completa los codewords del datamatrix.                               */
/************************************************************************/
void com_datamatrix(uchar n,uchar i)
{
   uchar j;
   char	*ptr;

   num_datamatrix = i;
   if (n >= i)							/* No falta ninguno */
     return;
   ptr = (char *)ORG_CODEWORDS;
   *(ptr+n) = 129;						/* Cter PAD para rellenar */
   n++;
   for (j=n;j<i;j++)
     {
     *(ptr+j) = calpad(j);
     }
}


/************************************************************************/
/* CALPAD                               20-Ene-10                       */
/* Calcula el caracter de relleno.                                      */
/************************************************************************/
uchar calpad(uchar i)
{
   int n;

   n = ((149 * i) % 253) + 1;
   n = 129 + n;
   if (n <= 254)
     return((uchar) n);
   return((uchar) (n - 254));
}

/************************************************************************/
/* CNV_DATAMATRIX                       01-Feb-10                       */
/* Convertir los caracteres especiales.                                 */
/************************************************************************/
uchar cnv_datamatrix(uchar c)
{
uchar const *tbl_cnvwin;

	switch(CodepageImpresion)
     {
       case CODEPAGE_850:
         tbl_cnvwin = tbl_cnvwin_850;
         break;
         
       case CODEPAGE_921:
         tbl_cnvwin = tbl_cnvwin_921;  	
         break;
         
       case CODEPAGE_852:
         tbl_cnvwin = tbl_cnvwin_852;
         
       case CODEPAGE_869:
         tbl_cnvwin = tbl_cnvwin_869;
         break;
         
       case CODEPAGE_866:
         tbl_cnvwin = tbl_cnvwin_866;
         break;
         
       case CODEPAGE_857:
         tbl_cnvwin = tbl_cnvwin_857;
         break;
         
       default:
         tbl_cnvwin = tbl_cnvwin_850;
         break;         
     }
	
   c = tbl_cnvwin[c-0x80];				/* Convertir el caracter de codepage de dos al de windows y en lower */
   if (c >= 0x80)
     c -= 0x80;
   return(c);
}

//**************************************************************************************
// CALRSS                               20-Nov-14                                      *
// Calcular RSS-14 Databar.                                                            *
//**************************************************************************************
void calrss(void)
{
   char  aux[8];
   uchar i;
   int   data1,data2,data3,data4;
   long  n,left,right;

   memmove(aux,&buf_auxiliar_parser[2],5);
   aux[5] = 0x00;
   right = 100000L + atol(aux);
   left = 0;
   for (i=0;i<8;i++)
     {
     n = 10L * right + buf_auxiliar_parser[7+i] - '0';
     left = 10 * left + (n / 4537077L);
     right = n % 4537077L;
     }
   data1 = (int) (left / 1597L);
   data2 = (int) (left % 1597L);
   data3 = (int) (right / 1597L);
   data4 = (int) (right % 1597L);
   memset(ingr,' ',46);
   ingr[46] = 0x00;
   ingr[0] = '1';
   ingr[1] = '1';
   ingr[44] = '1';
   ingr[45] = '1';
   calrss_1(data1,0);
   calrss_2(data2,0);
   calrss_1(data3,1);
   calrss_2(data4,1);
   calrss_chk();
}

//**************************************************************************************
// CALRSS_1                             20-Nov-14                                      *
// Calcular RSS-14 Databar.                                                            *
//**************************************************************************************
void calrss_1(int n,uchar m)
{
   uchar i,c,j;

   for (i=0;i<5;i++)
     {
     if (n <= tblrss_1[i][0])
       break;
     }
   if (i > 0)
     n -= tblrss_1[i-1][0] + 1;
   c = n / tblrss_1[i][5];
   getrsswidths(c,tblrss_1[i][1],4,tblrss_1[i][3],1);
   for (j=0;j<4;j++)
     ingr[tbl_pos_odd_1[m][j]] = rss_widths[j] + '0';
   c = n % tblrss_1[i][5];
   getrsswidths(c,tblrss_1[i][2],4,tblrss_1[i][4],1);
   for (j=0;j<4;j++)
     ingr[tbl_pos_even_1[m][j]] = rss_widths[j] + '0';
}

//**************************************************************************************
// CALRSS_2                             20-Nov-14                                      *
// Calcular RSS-14 Databar.                                                            *
//**************************************************************************************
void calrss_2(int n,uchar m)
{
   uchar i,c,j;

   for (i=0;i<4;i++)
     {
     if (n <= tblrss_2[i][0])
       break;
     }
   if (i > 0)
     n -= tblrss_2[i-1][0] + 1;
   c = n % tblrss_2[i][5];
   getrsswidths(c,tblrss_2[i][1],4,tblrss_2[i][3],1);
   for (j=0;j<4;j++)
     ingr[tbl_pos_odd_2[m][j]] = rss_widths[j] + '0';
   c = n / tblrss_2[i][5];
   getrsswidths(c,tblrss_2[i][2],4,tblrss_2[i][4],1);
   for (j=0;j<4;j++)
     ingr[tbl_pos_even_2[m][j]] = rss_widths[j] + '0';
}

//**************************************************************************************
// WRIRSS                               20-Nov-14                                      *
// Escribir RSS14 o RSS14 Truncado.                                                    *
//**************************************************************************************
void wrirss(void)
{
   int   h;
           			
   h = h_pos;							/* Salvar h_pos */
   //nbit = (uchar) (h_pos%8);
   inistr();							/* Init buffer de imagen */
   if ( densidad_ean > 2)
     densidad_ean = 0;
   dorss14(0,46,0x01,0,FALSE);
   dibean(0,RSS_14);					/* repite la primera linea hasta la altura del c�digo programada */
   h_pos = h;							/* Recuperar h_pos */
}

//**************************************************************************************
// CALRSS_CHK                           20-Nov-14                                      *
// Calcular el checksum.                                                               *
//**************************************************************************************
void calrss_chk(void)
{
   uchar i,left_check,right_check;
   uint  n;
   
   n = 0;
   for (i=0;i<46;i++)
     n += (ingr[i] & 0x0f) * tbl_chk_RSS14[i];
   n %= 79;								/* modulo 79 */
   if (n >= 8)
     n++;								/* se le suma 1 */
   if (n >= 72)
     n++;								/* se le suma 1 */
   left_check = n / 9;			
   right_check = n % 9;
   for (i=0;i<5;i++)					/* rellena los datos del "left finder pattern" de izqda a dcha */
     ingr[10+i] = tbl_anch_finder_pattern[left_check][i] + '0';
   for (i=0;i<5;i++)					/* rellena los datos del "right finder pattern" de dcha a izqda */
     ingr[31+i] = tbl_anch_finder_pattern[right_check][5-i-1] + '0';
}

//**************************************************************************************
// DORSS14                              20-Nov-14                                      *
// Hacer parte del RSS-14.                                                             *
//**************************************************************************************
uchar dorss14(uchar m,uchar n,uchar c,uchar modulo,uchar flg)
{
   uchar i,j,mask,d;

   for (i=m;i<n;i++)
     {
     if (c == 0x00)
       d = 1;
     else
       d = ingr[i] & 0x0f;
     for (j=0;j<d;j++)
       {
       mask = getmask(modulo);
       if (flg == TRUE)
         sepstr[indstr] &= ~mask;		
       else
         imastr[indstr] &= ~mask;		/* limpiamos los dots que vamos a colocar (2 dots por espacio) */
       if ((c == 0x00  &&  i == 0x00)  ||  (c == 0x01  &&  i & c))
         {
         if (flg == TRUE)
           sepstr[indstr] |= mask;		/* si posicion impar es una barra (2 dots por barra) */
         else
           imastr[indstr] |= mask;		/* si posicion impar es una barra (2 dots por barra) */
         }
       modulo = incmodulo(modulo);
       }
     }
   return(modulo);
}

//**************************************************************************************
// GETMASK                              21-Nov-14                                      *
// Coger mascara del RSS-14.                                                           *
//**************************************************************************************
uchar getmask(uchar m)
{
   switch (densidad_ean)
     {
     case 1:
       return(tbl_dato_barra_1[m]);
     case 2:
       return(tbl_dato_barra_2[m]);
     default:
       return(tbl_dato_barra[m]);
     }
}

//**************************************************************************************
// GETRSSWIDTHS                         21-Nov-14                                      *
// Calcular RSS-14 Databar.                                                            *
//**************************************************************************************
void getrsswidths(int val,int n,int elements,int maxwidth,int nonarrow)
{
   int bar,elmwidth,mxwelement,subval,lessval,narrowmask;

   narrowmask = 0;
   for (bar=0;bar<elements-1;bar++)
     {
     for (elmwidth=1,narrowmask |= (1 << bar); ;elmwidth++,narrowmask &= ~(1 << bar))
       {
       subval = combins(n-elmwidth-1,elements-bar-2);
       if ((!nonarrow) && (narrowmask == 0)  &&  (n-elmwidth-(elements-bar-1) >= elements-bar-1))
         subval -= combins(n-elmwidth-(elements-bar),elements-bar-2);
       if (elements-bar-1 > 1)
         {
         lessval = 0;
         for (mxwelement = n - elmwidth - (elements-bar-2);mxwelement > maxwidth;mxwelement--)
           lessval += combins(n-elmwidth-mxwelement-1,elements-bar-3);
         subval -= lessval * (elements-1-bar);
         }
       else
         {
         if (n-elmwidth > maxwidth)
           subval--;
         }
       val -= subval;
       if (val < 0)
         break;
       }
     val += subval;
     n -= elmwidth;
     rss_widths[bar] = elmwidth;
     }
   rss_widths[bar] = n;
}

//**************************************************************************************
// COMBINS                              21-Nov-14                                      *
// Calcular combinaciones.                                                             *
//**************************************************************************************
int combins(int n,int r)
{
   int i,j,max,min,val;

   if (n-r > r)
     {
     min = r;
     max = n - r;
     }
   else
     {
     min = n - r;
     max = r;
     }
   val = 1;
   j = 1;
   for (i=n;i>max;i--)
     {
     val *= i;
     if (j <= min)
       {
       val /= j;
       j++;
       }
     }
   for (;j<=min;j++)
     val /= j;
   return(val);
}

//**************************************************************************************
// INCMODULO                            21-Nov-14                                      *
// Incrementa modulo.                                                                  *
//**************************************************************************************
uchar incmodulo(uchar modulo)
{
   if ((densidad_ean == 0  &&  ++modulo > 3)  ||  (densidad_ean == 1  &&  ++modulo > 1)  ||  (densidad_ean == 2  &&  ++modulo > 7))         
     {
   	 modulo = 0;
   	 indstr++;
   	 }
   return(modulo);
}

/************************************************************************/
/* WRIRSS_APILADO                       09-Dic-10                       */
/* Escribir RSS14 o RSS14 apilado.                                      */
/************************************************************************/
void wrirss_apilado(void)
{
   uchar i,modulo,mask,ant_dato,alt_aux,indstr_aux1,indstr_aux2,indstr_aux3;
   int   h;
        			
   alt /= 2;
   h = h_pos;							/* Salvar h_pos */
   /* nbit = (uchar) (h_pos%8); */
   inistr();							/* Init buffer de imagen */
   modulo = dorss14(0,23,0x01,0,FALSE);
 /* meter la barra y el espacio adicional a la derecha de la primera fila */
   modulo = dorss14(0,2,0x00,modulo,FALSE);
   indstr_aux1 = indstr;				/* guarda indice de la PRIMERA PARTE */
   /* SEGUNDA PARTE DEL RSS-14 APILADO */
   indstr = 0;							/* meter la barra y el espacio adicional a la izquierda de la segunda fila */
   modulo = dorss14(0,2,0x00,0,TRUE);
   modulo = dorss14(23,46,0x01,modulo,TRUE);
   indstr_aux2 = indstr;				/* guarda indice de la SEGUNDA PARTE */
   modulo = 0;							/* Formacion del separador central */
   ant_dato = 0;						/* dato anterior en la linea de separacion */
   indstr = 1;							/* los 4 primeros modulos se ponen a cero */
/* los 4 ultimos modulos se ponen a cero */
/* son 50 modulos pero quitando los 4 ultimos (los primeros ya estan quitados con indstr = 1) */
/* 50 - 4 - 4 = 42 */
   for (i=0;i<42;i++)		
     {
     mask = getmask(modulo);
     if ((imastr[indstr] & mask) == (sepstr[indstr] & mask))	/* si modulos adyacentes vertical iguales */
       mask =  (~(imastr[indstr] & mask) & mask);
     else
       {
       if (modulo == 0)					/* dato anterior del tipo xxxx xx11 */
         ant_dato = ant_dato << 6;
       else								/* dato anterior del tipo 110011xx */
         ant_dato = ant_dato >> 2;
       ant_dato &= mask;
       ant_dato = ~ant_dato;
       ant_dato &= mask;
       mask = ant_dato;
       }
     sepstr1[indstr] |= mask;
     ant_dato = mask;
     modulo = incmodulo(modulo);
     }		
   indstr_aux3 = indstr;				/* guarda indice de SEPARADOR */
/**** AQUI EMPIEZA EL ENVIO AL PARSER DE LAS DIFERENTES PARTES DEL RSS-14 APILADO ********/
   alt_aux = alt;						/* guardamos la altura programada para hacer la ...... */
   alt = alt - 15;						/* diferencia de altura del TRUNCADO al APILADO (1� parte) */
   indstr = indstr_aux1;				/* recupera indice de la PRIMERA PARTE */
   indstr++;
   dibean(1,RSS_14);					/* repite la primera linea DE LA PRIMERA PARTE hasta la altura del c�digo programada */
/* aqui falta todo lo relacionado con las otras rotaciones */
   if (rotacion == 0)
	 v_pos += (alt + 1);
   if (rotacion == 2)
	 v_pos -= (alt + 1);
/***********************************************************/
   alt = 4;
   indstr = indstr_aux3;				/* recupera indice del SEPARADOR */
   indstr++;
   dibean(2,RSS_14);					/* repite la primera linea DEL MODULO SEPARADOR la altura indicada */
/* aqui falta todo lo relacionado con las otras rotaciones */
   if (rotacion == 0)
     v_pos += (alt + 1);
   if (rotacion == 2)
	 v_pos -= (alt + 1);
/***********************************************************/
   alt = alt_aux;
   alt = alt - 11;						/* diferencia de altura del TRUNCADO al APILADO (2� parte) */
   indstr = indstr_aux2;				/* recupera indice de la SEGUNDA PARTE */
   indstr++;
   dibean(3,RSS_14);					/* repite la primera linea DE LA SEGUNDA PARTE hasta la altura del c�digo programada */
   alt = alt_aux;
   h_pos = h;							/* Recuperar h_pos */
}

/************************************************************************/
/* WRIRSS_APILADO_OMNI                  10-Dic-10                       */
/* Escribir RSS14 apilado omnidireccional.                              */
/************************************************************************/
void wrirss_apilado_omni(void)
{
   uchar i,modulo,mask,ant_dato,alt_aux,indstr_aux1,indstr_aux2;
   int   h;
        			
   alt /= 2;
   h = h_pos;							/* Salvar h_pos */
   /* nbit = (uchar) (h_pos%8); */
   inistr();							/* Init buffer de imagen */
/* PRIMERA PARTE DEL RSS-14 APILADO */
   modulo = dorss14(0,23,0x01,0,FALSE);
/* meter la barra y el espacio adicional a la derecha de la primera fila */
   modulo = dorss14(0,2,0x00,modulo,FALSE);
   indstr_aux1 = indstr;				/* guarda indice de la PRIMERA PARTE */
/* SEGUNDA PARTE DEL RSS-14 APILADO */
   indstr = 0;							/* meter la barra y el espacio adicional a la izquierda de la segunda fila */
   modulo = dorss14(0,2,0x00,0,TRUE);
   modulo = dorss14(23,46,0x01,modulo,TRUE);
   indstr_aux2 = indstr;				/* guarda indice de la SEGUNDA PARTE */
   indstr = indstr_aux1;				/* recupera indice de la PRIMERA PARTE */
   indstr++;
   dibean(1,RSS_14);					/* repite la primera linea DE LA PRIMERA PARTE hasta la altura del c�digo programada */
/******************* Formacion de la PRIMERA PARTE DEL SEPARADOR CENTRAL ***************************************/
   modulo = 0;
   ant_dato = 0;						/* dato anterior en la linea de separacion */
   indstr = 1;							/* los 4 primeros modulos se ponen a cero */
   ant_dato = 0xff;						/* indicamos que es comienzo de proceso */
/* los 4 ultimos modulos se ponen a cero */
/* son 50 modulos pero quitando los 4 ultimos (los primeros ya estan quitados con indstr = 1) */
/* 50 - 4 - 4 = 42 */
   for (i=0;i<42;i++)		
     {
     mask = getmask(modulo);			/* cogemos la mascara que corresponde */
     sepstr1[indstr] &= ~mask;			/* limpiamos los dots que vamos a colocar (2 dots por espacio) */
     if (i >= 14  &&  i < 29) 			/* modulos correspondientes al finder pattern ? */
       {
       if ((imastr[indstr] & mask) == 0)	/* si este modulo es espacio */
         {
         if ((ant_dato == 0xff) || (ant_dato == 1))			/* si es primero o impar */
           {
           sepstr1[indstr] |= mask;		/* ponemos barra en el separador */
           ant_dato = 0;							/* e indicamos que el siguiente debe ser espacio */
           } 
         else
           ant_dato = 1;
         } 
       }		
     else
       {        	
       if ((imastr[indstr] & mask) == 0)	/* si este modulo es espacio */
         sepstr1[indstr] |= mask;		/* ponemos barra en el separador */
       }
     modulo = incmodulo(modulo);
     }		
   if (rotacion == 0)
	 v_pos += (alt + 1);
   if (rotacion == 2)
     {
	 v_pos -= alt + 1;
     h_pos -= 6;
     }
/* aqui se pinta LA PRIMERA PARTE DEL SEPARADOR CENTRAL */
   alt_aux = alt;						/* guardamos la altura programada  */
   alt = 4;
   dibean(2,RSS_14);					/* repite la primera linea del SEPARADOR hasta la altura del c�digo programada */
/******************* Formacion de la SEGUNDA PARTE DEL SEPARADOR CENTRAL ***************************************/
   for (i=0;i<BYTLINM;i++)
     sepstr1[i] = 0x00;
   modulo = 0;
   ant_dato = 0;			/* dato anterior en la linea de separacion */
   indstr = 1;				/* los 4 primeros modulos se ponen a cero */
/* los 4 ultimos modulos se ponen a cero */
/* son 50 modulos pero quitando los 4 ultimos (los primeros ya estan quitados con indstr = 1) */
/* 50 - 4 - 4 = 42 */
   for (i=0;i<42;i++)		
     {
     mask = getmask(modulo);			/* cogemos la mascara que corresponde */
     sepstr1[indstr] &= ~mask;			/* limpiamos los dots que vamos a colocar (2 dots por espacio) */
     if (i & 0x1)						/* posicion impar ? */  
       sepstr1[indstr] |= mask;			/* ponemos barra en el separador */
     modulo = incmodulo(modulo);
     }		
/* aqui se pinta LA SEGUNDA PARTE DEL SEPARADOR CENTRAL */
   if (rotacion == 0)
	 v_pos += alt;
   if (rotacion == 2)
     {
	 v_pos -= alt;
     h_pos -= 6;
     }
   dibean(2,RSS_14);					/* repite la primera linea del SEPARADOR hasta la altura del c�digo programada */
/******************* Formacion de la TERCERA PARTE DEL SEPARADOR CENTRAL ***************************************/
   for (i=0;i<BYTLINM;i++)
     sepstr1[i] = 0x00;
   modulo = 0;
   ant_dato = 0;			/* dato anterior en la linea de separacion */
   indstr = 1;				/* los 4 primeros modulos se ponen a cero */
   ant_dato = 0xff;	/* indicamos que es comienzo de proceso */
/* los 4 ultimos modulos se ponen a cero */
/* son 50 modulos pero quitando los 4 ultimos (los primeros ya estan quitados con indstr = 1) */
/* 50 - 4 - 4 = 42 */
   for (i=0;i<42;i++)		
     {
     mask = getmask(modulo);
     sepstr1[indstr] &= ~mask;		
     if (i >= 14  &&  i < 29) 			/* modulos correspondientes al finder pattern ? */
       {
       if ((sepstr[indstr] & mask) == 0)	/* si este modulo es espacio */
         {
         if ((ant_dato == 0xff) || (ant_dato == 1))			/* si es primero o impar */
           {
           sepstr1[indstr] |= mask;		/* ponemos barra en el separador */
           ant_dato = 0;				/* e indicamos que el siguiente debe ser espacio */
           } 
         else
           ant_dato = 1;
         } 
       }		
     else
       {        	
       if ((sepstr[indstr] & mask) == 0)	/* si este modulo es espacio */
         sepstr1[indstr] |= mask;		/* ponemos barra en el separador */
       }
     modulo = incmodulo(modulo);
     }		
/* aqui se pinta LA TERCERA PARTE DEL SEPARADOR CENTRAL */
   if (rotacion == 0)
	 v_pos += alt;
   if (rotacion == 2)
     {
	 v_pos -= alt;
     h_pos -= 6;
     }
   dibean(2,RSS_14);					/* repite la primera linea del SEPARADOR hasta la altura del c�digo programada */
/* aqui se pinta LA SEGUNDA PARTE DEL RSS-14 APILADO */
   if (rotacion == 0)
     {
     v_pos += alt;
     if (densidad_ean == 1)
       v_pos += 20;
     }
   if (rotacion == 2)
     {
	 v_pos -= alt;
     h_pos -= 6;
     }
   if (rotacion == 3)
     v_pos -= 15;
   alt = alt_aux;						/* recuperamos la altura programada  */
   indstr = indstr_aux2;				/* recupera indice de la SEGUNDA PARTE */
   indstr++;
   dibean(3,RSS_14);					/* repite la primera linea DE LA SEGUNDA PARTE hasta la altura del c�digo programada */
   h_pos = h;							/* Recuperar h_pos */
}

/************************************************************************/
/* WRI3D9                               10-Ene-12                       */
/* Escribir el cter en el tipo adecuado. 3D9.                           */
/************************************************************************/
static	void wri3d9(uchar c)
{
   uchar i;

   if (c < ' '  ||  c > 'Z')
     return;
   c -= ' ';
//   SET_BNK_ROM(BNKROM_0);
   i = tbl3d9[2*c];
   if (i == 0x00)
     return;
   imaean(i,8);
//   SET_BNK_ROM(BNKROM_0);
   i = tbl3d9[(2*c)+1];
   if (i == 0x00)
     return;
   imaean(i,5);
   num128++;
}

/************************************************************************/
/* WRIRSS_EXPANDIDO                     10-Sep-13                       */
/* Escribir Databar Expandido.                                          */
/************************************************************************/
void wrirss_expandido(uchar c)
{
   char *s;
   uchar num_symbols;
   int espacioNecesario,Espacio;
   
   Espacio = 0;

#if CORR_DATABAR_EXP_STACK
   memset(ingr,0x00,46);
#endif
   memmove(str,buf_auxiliar_parser,256);
   s = (char *)buf_auxiliar_parser;
   stacked = 0;
   extrapadding = 0;
   rss_expanded_data(s);
   num_symbols = 1 + (ind_bitstr / 12);
   if (num_symbols > 22)				/* No caben mas de 22 simbolos en RSS Expandido */
     return;
   stacked = 0;
   num_rows = 0;
   if (datos_rss14 == 0)
     c = 0;
   switch (c)
     {
     case 0: 							/* Expandido */
       row_symbols = num_symbols;
       num_rows = 1;
       break;
     case 1: 							/* Expandido apilado 8 simbolos por linea */
       row_symbols = 8;
       if (num_symbols > 8)
         stacked = 1;
       break;
     case 2: 							/* Expandido apilado 4 simbolos por linea */
       row_symbols = 4;
       if (num_symbols > 4)
         stacked = 1;
       break;
     case 3: 							/* Expandido apilado n simbolos por linea */
       row_symbols = datos_rss14;
       if (num_symbols > datos_rss14)
         stacked = 1;
       break;
     }
   switch (rotacion)
	 {
     case 0:
       Espacio = xpag - hposter;
       break;
     case 1:
       Espacio = ypag - v_pos;
       break;
     case 2:
       Espacio = hposter;
       break;
     case 3:
       Espacio = v_pos;
       break;
     }

   espacioNecesario = 2*(row_symbols*17 + (row_symbols/2)*15 + (row_symbols%2)*15);
   if (espacioNecesario > Espacio)
     {
     stacked = 1;
     row_symbols = 2*(Espacio/100);
     }	
   if (stacked == 1)
     {									/* Si hay que stackear, codificar los datos */
     memmove(buf_auxiliar_parser,str,256);
     s = (char *)buf_auxiliar_parser;
     rss_expanded_data(s);
     calcula_databar_expanded_stacked();
	 }
   else
     calcula_databar_expanded();
   PintaCodigoRSSExpandido();
   mostrar_ingr();
}

/************************************************************************/
/* RSS_EXPANDED_DATA                    05-Sep-13                       */
/* Codificar el expandido.                                              */
/************************************************************************/
void rss_expanded_data(char *s)
{
   uchar estado;

   estado = 0;							/* 0 = Numeric, 1 = Alphanumeric, 2 = ISO/IEC 646 3 = fin */
   ini_bitstr();
   ins_bitstr(0x00,1);					/* Linkage flag */
   ins_bitstr(0x00,2);					/* Encodation method */
   ins_bitstr(0x00,2);					/* Lenght bits */
   while (estado != 3)
     {
     switch (estado)
       {
       case 0:   
         s += numeric_encode(&estado,s);
         break;
       case 1:   
         s += alphanumeric_encode(&estado,s);
         break;
       case 2:   
         s += iso_iec_encode(&estado,s);
         break;
       default: 
         estado = 3;
         break;
       }
     if (ind_bitstr > 264) 
       {
       ind_bitstr = 264;
       return;
       }     	
     }
   if ((1 + ind_bitstr / 12) % 2 == 0) 	/* Numero par de simbolos, primer bit del campo variable length symbol a 0 */
     pon_bitstr(3,0);
   else
     pon_bitstr(3,1);
   if ((1 + ind_bitstr / 12) <= 14) 	/* Menos o igual de 14 simbolos, segundo bit del campo variable length symbol a 0 */
     pon_bitstr(4,0);
   else
     pon_bitstr(4,1);
   mostrar_bitstr();
}

/************************************************************************/
/* PINTACODIGORSSEXPANDIDO   			10-Sep-13                       */
/* Dibuja el Databar Expandido.                                         */
/************************************************************************/
void PintaCodigoRSSExpandido(void)
{
   uchar c;
   int i,k,h,v;

   inistr();                            /* Reset buffer imagen */
   if (stacked == 0)
     {
     i = 0;
     while (1)
       {
       c = ingr[i];
       if (c == 0x00)
         break;
       if ((i % 2) == 0)				/* Posicion par, es espacio */
         imaean3(0x00,c);
       else								/* Posicion impar, es barra */
         imaean3(0xff,c);
       i++;
       }

#if !CORR_EAN_ROTADO_180
     if (rotacion == 2)
       h_pos -= indstr * 8;
#endif
#if !CORR_EAN_ROTADO_90
     if (rotacion == 3)
       v_pos -= indstr * 8;
#endif

     indstr++;
  #if CORR_EAN_COMO_LP_2
	  dibean(8,E_EXPAND);
  #else
     dibean(8,E_128);
  #endif
     return;
	 }
#if !CORR_EAN_ROTADO_180
   if (rotacion == 2)
     h_pos -= bits_in_row * 2;
#endif
#if !CORR_EAN_ROTADO_90
   if (rotacion == 3)
     v_pos -= bits_in_row * 2;
#endif
   for (k=0;k<num_rows;k++)				/* Apilado */
 	 {									/* BARRAS */
     inistr();                        	/* Reset buffer imagen */
	 for (i=bits_in_row*k;i<bits_in_row*k+((k==num_rows-1) ? bits_in_last_row : bits_in_row);i++)
       {
	   if (rd_barras_stacked(i) == 0)
	     imaean3(0x00,1);
       else
	     imaean3(0xff,1);
       }
     h = h_pos;
     v = v_pos;
     indstr++;
#if CORR_EAN_COMO_LP_2
	 dibean(8,E_EXPAND);
#else
     dibean(8,E_128);
#endif
     h_pos = h;
     v_pos = v;
     if (k < num_rows - 1)
	   CorreccionPosicionRotacion(alt);
#if !CORR_EAN_ROTADO_180
     if (rotacion == 2  &&  k == num_rows - 2)
       h_pos += (bits_in_row - bits_in_last_row)* 2;
#endif
#if !CORR_EAN_ROTADO_90
     if (rotacion == 3  &&  k == num_rows - 2)
       v_pos += (bits_in_row - bits_in_last_row)* 2;
#endif
     if (k != num_rows - 1)				/* Separadores */
       {
       sepsup(k);
       sepmed(k);
       sepinf(k);
       }
 	 }
}

/************************************************************************/
/* CORRECCIONPOSICIONROTACION			16-Sep-13                       */
/* Calcula la posicion de la siguiente fila.                            */
/************************************************************************/
void CorreccionPosicionRotacion(int Offset)
{
   switch (rotacion)
     {
     case 0:
       v_pos += Offset;
       break;
     case 1:
       h_pos -= Offset;
       break;
     case 2:
       v_pos -= Offset;
       break;
     case 3:
       h_pos += Offset;
       break;
     default:
       break;
     }
}

/************************************************************************/
/* ALPHANUMERIC_ENCODE                  05-Sep-13                       */
/* Codifica las letras.                                                 */
/************************************************************************/
int alphanumeric_encode(uchar *estado,char *s)
{
   int i;
   uint numero_simbolos,numericos,resto,procesados = 0;
   long padding_bits; 

   procesados = 0;
   padding_bits = 0x0421084L; 			/* Bits de padding caso alfanumerico, en orden inverso */
   numericos = cuantos_numericos(s);
   if (strlen(s) == 0) 					/* Ya consumi el ultimo caracter, relleno con los bits de padding */
     {
     resto = ind_bitstr % 12;
     if (resto != 0) 					/* Si no hay un numero exacto de caracteres, calculo cuanto hay que a�adir */
       resto = 12 - resto;
     if (stacked == 1)
       {								/* Calculo cuantos caracteres va a haber */
       numero_simbolos = 1 + ind_bitstr / 12; 		
       numero_simbolos = numero_simbolos + (resto == 0 ? 0 : 1); 	
       extrapadding = numero_simbolos % row_symbols;  
       }
     if (extrapadding == 1)
       resto += 12;  					/* A�ado 12 bits m�s de padding */
     for(i=0;i<resto;i++)
       {
       ins_bitstr(padding_bits,1);
       padding_bits = padding_bits >> 1;
       }
     *estado = 3; 						/* Estado final */
     }
   else if (numericos >= 6)
     {
     ins_bitstr(0x00,3); 				/* Inserto el numeric latch y paso al estado 0 */
     *estado = 0;
     }
   else if (numericos >= 4  &&  strlen(s) == numericos) 
     {									/* Si quedan 4 o 5 y son numericos */
     ins_bitstr(0x00,3); 				/* Inserto el numeric latch y paso al estado 0 */
     *estado = 0;
     }
   else if (no_es_alfanumerico(s[0]))
     {
     ins_bitstr(0x04,5); 				/* Inserto el ISO_IEC latch y paso al estado 2 */
     *estado = 2;
     }
   else if (s[0] == FNC1) 				/* Inserto el FNC1 y paso al estado 0 */
     {
     ins_bitstr(0x0F,5);
     *estado = 0;
     procesados = 1; 					/* Un caracter procesado */
     }
   else  								/* Es un caracter del juego alfanum�rico */
     {
     if (s[0] >= '0'  &&  s[0] <= '9') 	/* Digito 0-9 */
       {
       ins_bitstr(s[0] - 43,5);
       procesados = 1; 					/* Un caracter procesado */
       *estado = 1; 					/* Sigo en el estado 1 */
       }
     else if (s[0] >= 'A'  &&  s[0] <= 'Z') 
       {								/* Letra mayuscula A-Z */
       ins_bitstr(s[0] - 33,6);
       procesados = 1; 					/* Un caracter procesado */
       *estado = 1; 					/* Sigo en el estado 1 */
       }
     else if (s[0] >= 44  &&  s[0] <= 47) 
       {								/* ',' '-' '.' o '/' */
       ins_bitstr(s[0] + 15, 6);
       procesados = 1; 					/* Un caracter procesado */
       *estado = 1; 					/* Sigo en el estado 1 */
       }
     else if (s[0] == '*') 				/* '*' */
       {
       ins_bitstr(58,6);
       procesados = 1; 					
       *estado = 1; 					
       }
     }
   return(procesados);
}

/************************************************************************/
/* CALCULA_DATABAR_EXPANDED             10-Sep-13                       */
/* RSS Expandido.                                                       */
/************************************************************************/
void calcula_databar_expanded(void)
{
   uchar aux_anchos[8]; 				/* Para guardar los anchos parciales que se van sacando */
   uint num_data_elements; 				/* Numero de datos que se van a codificar */
   uint num_symbol_elements; 			/* Numero de simbolos totales (icluido el check char) */
   uint p; 								/* Peso a utilizar en el calculo del check_char */
   int i;  								/* Contador del bucle principal. NO USAR EN NINGUN OTRO BUCLE */
   int j;
   uint check_char,check_sum;
   uint fila_finders; 					/* Fila de la tabla tbl_finders_RSS_expanded que se va a usar */
   uint next_finder; 					/* Indice del finder que toca utilizar */
   uint indice_tabla_pesos;  			/* Indice auxiliar para acceder a la tabla de pesos */
   uint indice_simbolo; 				/* Indice del simbolo que se est� procesando */
   uint indice_modulo; 					/* Indice que lleva la cuenta del modulo del que toca poner el ancho */

   if (ind_bitstr % 12 != 0)
     return;
   check_sum = 0;
   next_finder = 0;
   indice_simbolo = 1;
   indice_modulo = 0;
   num_data_elements = ind_bitstr / 12;
   num_symbol_elements = num_data_elements + 1;
   fila_finders = (num_symbol_elements - 3) / 2;
   ingr[indice_modulo++] = 1; 			/* Primer hueco */
   ingr[indice_modulo++] = 1; 			/* Primera barra */
   indice_simbolo++;					/* Dejar hueco para el check char */
   indice_modulo += 8;
   for (j=0;j<5;j++)					/* Colocar primer finder (Siempre es A1) */
     ingr[indice_modulo++] = tbl_anchos_finders_RSS_expanded[tbl_finders_RSS_expanded[fila_finders][next_finder]][j];
   next_finder++;
   ind_bitstr = 0;
   for (i=0;i<num_data_elements;i++)
     {									/* Calcular elemento */
     anchos_valor_expanded(ext_bitstr(),(char *)aux_anchos);
     indice_tabla_pesos = 2*(tbl_finders_RSS_expanded[fila_finders][(indice_simbolo-1)/2]); /* Me da el finder m�s cercano */
     if (indice_simbolo % 2 == 0)
       indice_tabla_pesos++;			/* Si el simbolo es par, est� a la derecha del finder */
     p = tbl_weights_RSS_expanded[indice_tabla_pesos];
     for (j=0;j<8;j++)
       {
       check_sum += aux_anchos[j]*p;
       p = (p * 3) % 211;
       }
     if (indice_simbolo % 2 == 0) 		/* Si es un simbolo par va de derecha a izquierda */
       {
       for (j=7;j>=0;j--)
         ingr[indice_modulo++] = aux_anchos[j];
       }
     else 								/* Si es un simbolo impar va de izquierda a derecha */
       {
       for (j=0;j<=7;j++)
         ingr[indice_modulo++] = aux_anchos[j];
       }
     if (indice_simbolo % 2 != 0) 		/* Si es un simbolo impar, a continuaci�n va un finder */
       {
       for (j=0;j<5;j++)
         ingr[indice_modulo++] = tbl_anchos_finders_RSS_expanded[tbl_finders_RSS_expanded[fila_finders][next_finder]][j];
       next_finder++;
       }
     indice_simbolo++;
     }
   ingr[indice_modulo++] = 1;			/* Ultima barra */
   ingr[indice_modulo++] = 1;			/* Ultimo hueco */
   ingr[indice_modulo] = 0x00; 			/* Para poder tratar todo el array como un string */
   check_sum = check_sum % 211;			/* Checksum total */
   check_char = (211 * (num_symbol_elements - 4)) + check_sum;
   anchos_valor_expanded(check_char,(char *)aux_anchos);
   for (j=0;j<8;j++)
     ingr[2+j] = aux_anchos[j];
}

/************************************************************************/
/* ANCHOS_VALOR_EXPANDED  				10-Sep-13                       */
/* Calcular los datos de un valor.                                      */
/************************************************************************/
void anchos_valor_expanded(uint valor,char *anchos)
{
   int i;
   uint limite,divisor,impar,par,modulos, anch_max;
   char anchos_par[4],anchos_impar[4];
  
   for (i=1;i<=5;i++)					/* Busca en la tabla en que grupo est� el valor */
     {
     limite = tbl_data_RSS_expanded[5-i][0];
     divisor = tbl_data_RSS_expanded[5-i][5];
     if (valor >= limite)
       {
       impar = (valor - limite) / divisor;
       modulos = tbl_data_RSS_expanded [5-i][1];		/* numero de modulos del subconjunto impar */
       anch_max = tbl_data_RSS_expanded [5-i][3];		/* anchura del modulo del subconjunto impar */
       get_Rss14_anchura_elemento((uchar *)anchos_impar,impar,modulos,4,anch_max,0);
       par = (valor - limite) % divisor;
       modulos = tbl_data_RSS_expanded [5-i][2];		/* numero de modulos del subconjunto par */
       anch_max = tbl_data_RSS_expanded [5-i][4];		/* anchura del modulo del subconjunto par */
       get_Rss14_anchura_elemento((uchar *)anchos_par,par,modulos,4,anch_max,1);
       for (i=0;i<8;i=i+2)
         {
         anchos[i] = anchos_impar[i/2];
         anchos[i+1] = anchos_par[i/2];
         }
       break;
       }
     }
}

/************************************************************************/
/* CALCULA_DATABAR_EXPANDED_STACKED		10-Sep-13                       */
/* Dibuja el Databar Expandido Apilado.                                 */
/************************************************************************/
void calcula_databar_expanded_stacked(void)
{
   int h, i, j, k;
   int limite; 
   int last_row_symbols; 
   int filas_pares_reversed; 			/* Indica si las filas pares hay que darles la vuelta (1) o no (0) */

   cal_row();
   if (row_symbols == 0)
     {
     num_rows = 1;
     last_row_symbols = 1;
     }
   else
     {
     num_rows = (1 + ind_bitstr/12) / row_symbols;  /* El 1 + es por el check char */
     last_row_symbols = (1 + (ind_bitstr)/12) % row_symbols;	/* El 1 + es por el check char */
     }
   if (last_row_symbols == 0) 			/* Si el resto de la operacion anterior es 0, la ultima fila est� completa */
     last_row_symbols = row_symbols;
   calcula_databar_expanded();
   calcula_separadores_bruto();
   calcula_codigo_barras_bruto();
   bits_in_row = 49 * (row_symbols / 2);
   bits_in_last_row = 49 * (last_row_symbols / 2);
   if (last_row_symbols % 2 == 1) 		/* Si en la ultima fila hay un numero impar de simbolos */
     bits_in_last_row += 32; 			/* Hay un simbolo menos, sumo 17 bits de ese simbolo mas 15 del finder pattern */
   if (last_row_symbols != 0  &&  last_row_symbols != row_symbols  &&  num_rows >= 1)	 /* Si hay una ultima fila no completa */
     num_rows += 1;
   if (num_rows < 2)
     return; 							/* Error. No hay suficientes caracteres para hacer stacked */
   if (row_symbols % 4 == 0)
     filas_pares_reversed = 1;
   else
     filas_pares_reversed = 0;
   j = 0; 								/* Indice de escritura del codigo de barras final */
   k = 0; 								/* Indice de lectura del codigo de barras original */
   for (i=0;i<num_rows;i++)
     {
     if (i == 0) 						/* Fila 0, ya tiene el hueco y barra iniciales */
       {
       wr_barras_stacked(j++,rd_codigo_barras(k++));
       wr_barras_stacked(j++,rd_codigo_barras(k++));
       }
     else 								/* En otras filas tengo que meter el hueco y barra iniciales */
       {
       if (((i+1) % 2 == 0) &&  (filas_pares_reversed == 1)  &&  (i == num_rows - 1)  &&  (last_row_symbols % 4 != 0))
         {								/* Caso especial: ultima fila con numero impar de finders: no se le da la vuelta y se mete un hueco al inicio */
         wr_barras_stacked(j++,0); 		/* Hueco */
         j += hueco_barra(j);
         }
       else if ((i+1) % 2 == 0) 		/* Si es fila par es barra y hueco */
         j += barra_hueco(j);
       else
         j += hueco_barra(j);
       }
     /* Ya est�n a�adidos los puntos iniciales. Ahora se copian los bits correspondientes al codigo de barras */
     limite = (i == (num_rows - 1)) ? bits_in_last_row : bits_in_row; 	/* Cuantos bits hay que copiar */
     if (((i+1) % 2 == 0)  &&  (filas_pares_reversed == 1)  &&  (i == num_rows - 1)  &&  (last_row_symbols % 4 != 0))
     /* Caso especial: ultima fila con numero impar de finders: no se le da la vuelta y se mete un hueco al inicio */
       {
       for (h=0;h<limite;h++)
         {
         wr_barras_stacked(j,rd_codigo_barras(k));
         wr_separadores_stacked(j++,rd_separadores(k++));
         }
       }
     else if (((i+1) % 2 == 0)  &&  (filas_pares_reversed == 1) ) 	/* Si es fila par, y hay que darle la vuelta a las pares */
       {
       for (h=0;h<limite;h++)
         {
         wr_barras_stacked(j+limite-1-h,rd_codigo_barras(k));
         wr_separadores_stacked(j+limite-1-h,rd_separadores(k));
         k++;
         }
       j += limite;
       }
     else  								/* Si es fila impar */
       {
       for (h=0;h<limite;h++)
         {
         wr_barras_stacked(j,rd_codigo_barras(k));
         wr_separadores_stacked(j++,rd_separadores(k++));
         }
       }
     /* Ahora se a�aden el hueco y barra, o barra y hueco finales */
     if (((i+1) % 2 == 0)  &&  (filas_pares_reversed == 1)  &&  (i == num_rows - 1)  &&  (last_row_symbols % 4 != 0))
     /* Caso especial: ultima fila con numero impar de finders: no se le da la vuelta y se mete un hueco al inicio */
       {
       j += barra_hueco(j);
       bits_in_last_row++; 				/* En este caso la ultima fila ha crecido en un bit m�s */
       }
     else if ((i+1) % 2 == 0 ) 			/* Si es fila par */
       {
       if ((((i == num_rows -1) ? last_row_symbols : row_symbols) % 4 == 0)  ||  ((((i == num_rows -1) ? last_row_symbols : row_symbols) + 1) % 4) == 0)
         j += barra_hueco(j);
       else
         j += hueco_barra(j);
       }
     else  								/* Si es fila impar */
       {
       if ((((i == num_rows -1) ? last_row_symbols : row_symbols) % 4 == 0) ||  ((((i == num_rows -1) ? last_row_symbols : row_symbols) + 1) % 4) == 0)
         j += hueco_barra(j);
       else
         j += barra_hueco(j);
      }
     }
   bits_in_row += 4; 					/* Le a�ado 4 porque ya tiene los huecos y barras inicial y final cada fila */
   bits_in_last_row += 4; 				/* Le a�ado 4 porque ya tiene los huecos y barras inicial y final cada fila */
   /* Se ponen los 4 primeros y 4 ultimos bits de cada separador a 0 */
   for (j=0;j<num_rows;j++)
     {									/* 4 primeros */
     for (h=j*bits_in_row;h<j*bits_in_row+4;h++)
       wr_separadores_stacked(h,0);
     limite = (j == (num_rows - 1)) ? bits_in_last_row : bits_in_row;	/* 4 ultimos */
     for (h=j*bits_in_row+limite-1;h>j*bits_in_row+limite-1-4;h--)
       wr_separadores_stacked(h,0);
     }
}

/************************************************************************/
/* HUECO_BARRA                     		10-Sep-13                       */
/* Hueco y barra.                                                       */
/************************************************************************/
int hueco_barra(int j)
{
   wr_barras_stacked(j++,0); 			/* Hueco */
   wr_barras_stacked(j,1); 				/* Barra */

   return(2);
}

/************************************************************************/
/* BARRA_HUECO                     		10-Sep-13                       */
/* Barra y hueco.                                                       */
/************************************************************************/
int barra_hueco(int j)
{
   wr_barras_stacked(j++,1); 			/* Barra */
   wr_barras_stacked(j,0); 				/* Hueco */
   return(2);
}

/************************************************************************/
/* CAL_ROW                   			16-Sep-13                       */
/* Calcula el numero de filas para dejarlo centrado.                    */
/************************************************************************/
void cal_row(void)
{
   int n,h,lnr,lnra;

   if (num_rows == 0  ||  row_symbols == 0)
     return;
   h = (1 + ind_bitstr/12);
   n = row_symbols;
   lnra = 0;
   while (1)
     {
     lnr = h % n;	
     if (lnr == 0  ||  n <= 2  ||  lnra > lnr)
       {
       row_symbols = n;
       break;
       }
     if (lnr > n)
       {
       row_symbols = n + 2;
       break;
       }
     lnra = n;
     n -= 2;
     }
}

/************************************************************************/
/* CALCULA_CODIGO_BARRAS_BRUTO     		12-Sep-13                       */
/* Calcula los dots del codigo de barras.                               */
/************************************************************************/
int calcula_codigo_barras_bruto(void)
{
   uchar blanco_negro;
   int i,j,num_anchos,indice_dot; 

   num_anchos = strlen(ingr);
   blanco_negro = 0; 					/* Empiezo en hueco */
   indice_dot = 0;
   for (i=0;i<num_anchos;i++)
     {
     for (j=0;j<ingr[i];j++)
       wr_codigo_barras(indice_dot++,blanco_negro);
     if (blanco_negro == 0)
       blanco_negro = 1;
     else
       blanco_negro = 0;
     }
   return(indice_dot);
}

/************************************************************************/
/* CALCULA_SEPARADORES_BRUTO       		12-Sep-13                       */
/* Calcula los separadores.                                             */
/************************************************************************/
void calcula_separadores_bruto(void)
{
   uchar blanco_negro;  				/* 0 => blanco ; 1 => negro */
   int i,k,num_anchos,indice_bit_separador,alternador;
   int finder; 							/* Indice del finder para saber si es par o impar */

    num_anchos = strlen(ingr);
    blanco_negro = 1; 					/* Empiezo en negro */
    alternador = 1; 					/* Empieza en negro */
    indice_bit_separador = 0;
    finder = 0;
    for (i=0;i<num_anchos;i++)
      {
      if (i == indices_finders[finder])
        {
        for (k=0;k<ingr[i];k++)
          {
          wr_separadores(indice_bit_separador,alternador);
          indice_bit_separador++;
          if (alternador == 0)
            alternador = 1;
          else
            alternador = 0;
          }
        alternador = 1; 				/* Vuelvo a negro */
        finder++;
        }
      else
        {
        for (k=0;k<ingr[i];k++)
          wr_separadores(indice_bit_separador++,blanco_negro);
        }
      if (blanco_negro == 0)
        blanco_negro = 1;
      else
        blanco_negro = 0;
      }
}

/************************************************************************/
/* CUANTOS_NUMERICOS                    09-Sep-13                       */
/* Devuelve el numero de caracteres consecutivos numericos.             */
/************************************************************************/
int cuantos_numericos(char *s)
{
   int i;

   for(i=0;i<=strlen(s);i++)
     {
     if ((s[i] >= '0'  &&  s[i] <= '9')  ||  s[i] == FNC1)
       continue;
     break;
     }
   return(i);
}

/************************************************************************/
/* EXT_BITSTR             				10-Sep-13                       */
/* Extraer 12 bits del bit str.                                         */
/************************************************************************/
uint ext_bitstr(void)
{
   int i;
   uint resultado;
   char	*ptr;

   resultado = 0;
   ptr = (char *)ORG_DATAMATRIX;
   for (i=0;i<12;i++)
     {
     resultado = resultado << 1;
     resultado = resultado | *(ptr+ind_bitstr);
     ind_bitstr++;
     }
   return(resultado);
}

/************************************************************************/
/* GET_RSS14_ANCHURA_ELEMENTO			10-Sep-13                       */
/* Calcula la anchura de los elementos del RSS para un valor dado       */
/************************************************************************/
void get_Rss14_anchura_elemento(uchar *anch_elemento,int val,int n,int num_elemento,int anch_max,int noNarrow)
{
   uchar i;
   int bar,elem_anch,max_elemento,subVal,lessVal,narrowMask;

   for (i=0;i<4;i++)
     anch_elemento[i] = 0;
   narrowMask = 0;
   for (bar=0;bar<num_elemento-1;bar++)	
     {
     for (elem_anch=1,narrowMask |= (1<<bar); ;elem_anch++,narrowMask &= ~(1<<bar))
       {								/* coge todas las combinaciones */
       subVal = combins(n - elem_anch - 1 , num_elemento -2 - bar);
       if ((!noNarrow) && (narrowMask == 0) && (n - elem_anch - (num_elemento - 1 -bar) >= num_elemento -1 -bar))
         subVal -= combins(n-elem_anch - (num_elemento - bar),num_elemento-2 - bar);
       if (num_elemento -1 - bar > 1)
         {
         lessVal = 0;
         for (max_elemento = n-elem_anch-(num_elemento -2 - bar); max_elemento > anch_max ; max_elemento--)
           lessVal += combins(n - elem_anch - max_elemento-1 , num_elemento -3 - bar);
         subVal -= lessVal * (num_elemento -1 - bar);
         }
       else if (n - elem_anch > anch_max)
         subVal --;
       val -= subVal;
       if (val < 0)
         break;
       }
     val += subVal;
     n -= elem_anch;
     anch_elemento[bar] = (char)elem_anch;
     }
   anch_elemento[bar] = (char)n;
}

/************************************************************************/
/* INI_BITSTR                           05-Sep-13                       */
/* Inicializa el data string del databar expandido.                     */
/************************************************************************/
void ini_bitstr(void)
{
   int i;
   char	*ptr;

   ptr = (char *)ORG_DATAMATRIX;
   for (i=0;i<272;i++)
     *ptr++ = 0x00;
   ind_bitstr = 0;
}

/************************************************************************/
/* INS_BITSTR                           05-Sep-13                       */
/* Insertar bits en el data string del databar expandido.               */
/************************************************************************/
void ins_bitstr(uint data,uint nbits)
{
   int i;
   uint mask;
   char	*ptr;

   mask = 0x01;
   ptr = (char *)ORG_DATAMATRIX;
   ptr += ind_bitstr;
   for (i=nbits-1;i>=0;i--)
     {
     *(ptr+i) = data & mask;
     data = data >> 1; 
     }
   ind_bitstr += nbits;
}

/************************************************************************/
/* ISO_IEC_ENCODE                       09-Sep-13                       */
/* Codificando caracteres utilizando el juego de caracteres iso/iec 646.*/
/************************************************************************/
int iso_iec_encode(uchar *estado,char *s)
{
   int i;
   uint numero_simbolos,resto,procesados;
   long padding_bits; 

   procesados = 0;
   padding_bits = 0x0421084L; 
   if (strlen(s) == 0) 					/* Ya consumi el ultimo caracter, relleno con los bits de padding */
     {
     resto = ind_bitstr % 12;
     if (resto != 0) 					/* Si no hay un numero exacto de caracteres, calculo cuanto hay que a�adir */
       resto = 12 - resto;
     if (stacked == 1)
       {
       numero_simbolos = 1 + ind_bitstr / 12;
       numero_simbolos = numero_simbolos + (resto == 0 ? 0 : 1); 
       extrapadding = numero_simbolos % row_symbols;
       }
     if (extrapadding == 1)
       resto += 12;  					/* A�ado 12 bits m�s de padding */
     for (i=0;i<resto;i++)
       {
       ins_bitstr(padding_bits,1);
       padding_bits = padding_bits >> 1;
       }
     *estado = 3; 						/* Estado final */
     }
   else if (s[0] == FNC1) 				/* Inserto el FNC1 y paso a estado 0 */
     {
     ins_bitstr(0x0F,5);
     *estado = 0;
     procesados = 1;
     }
   else if ((cuantos_numericos(s) >= 4)  &&
           ((cuantos_no_iso_iec(s) == 10)  ||  (cuantos_no_iso_iec(s) == strlen(s))))
     {
     ins_bitstr(0x00,3); 				/* Inserto el numeric latch y paso al estado 0 */
     *estado = 0;
     }
   else if ((cuantos_alfanumericos(s) >= 5) &&
           ((cuantos_no_iso_iec(s) == 10)  ||  (cuantos_no_iso_iec(s) == strlen(s))))
     {
     ins_bitstr(0x04,5); 				/* Inserto el alfanumeric latch y paso al estado 1 */
     *estado = 1;
     }
   else if (s[0] >= '0'  &&  s[0] <= '9')
     {
     ins_bitstr(s[0] - 43,5);
     *estado = 2;
     procesados = 1;
     }
   else if (s[0] >= 'A'  &&  s[0] <= 'Z')
     {
     ins_bitstr(s[0] - 1,7);
     *estado = 2;
     procesados = 1;
     }
   else if (s[0] >= 'a'  &&  s[0] <= 'z')
     {
     ins_bitstr(s[0] - 7,7);
     *estado = 2;
     procesados = 1;
     }
   else if (s[0] == 33  ||  s[0] == 34) /* ! o " */
     {
     ins_bitstr(s[0] + 199,8);
     *estado = 2;
     procesados = 1;
     }
   else if (s[0] >= 37  &&  s[0] <= 47) /* % & ' ( ) * + , - . / */
     {
     ins_bitstr(s[0] + 197,8);
     *estado = 2;
     procesados = 1;
     }
   else if (s[0] >= 58  &&  s[0] <= 63) /* : ; < = > ? */
     {
     ins_bitstr(s[0] + 187,8);
     *estado = 2;
     procesados = 1;
     }
   else if (s[0] == '_')
     {
     ins_bitstr(251,8);
     *estado = 2;
     procesados = 1;
     }
   else if (s[0] == ' ')
     {
     ins_bitstr(252,8);
     *estado = 2;
     procesados = 1;
     }
   return(procesados);
}

/************************************************************************/
/* CUANTOS_ALFANUMERICOS                09-Sep-13                       */
/* Cters consecutivos codificables con el juego alfanumerico.           */
/************************************************************************/
int cuantos_alfanumericos(char *s)
{
   int i;

   for (i=0;i<strlen(s);i++)
     {
     if (es_alfanumerico(s[0]))
       continue;
     break;
     }
  return(i);
}

/************************************************************************/
/* CUANTOS_NO_ISO_IEC                   09-Sep-13                       */
/* Cters que se pueden codificar con juego distinto de ISO/IEC 646      */
/************************************************************************/
int cuantos_no_iso_iec(char *s)
{
   int i,max;

   max = 10;
   if (strlen(s) < 10)
     max = strlen(s);
   for (i=0;i<max;i++)
     {
     if (!es_alfanumerico(s[i]))
       break;
     }
   return(i);
}

/************************************************************************/
/* ES_ALFANUMERICO                      09-Sep-13                       */
/* Devuelve si el caracter es codificable con el juego alfanumerico.    */
/************************************************************************/
uchar es_alfanumerico(uchar c)
{
   if ((c >= '0'  &&  c <= '9')  ||
       (c == FNC1)  ||
       (c >= 'A'  &&  c <= 'Z')  ||
       (c == '*')  ||
       (c >= 44  &&  c <= 47))
     return(TRUE);
   return(FALSE);
}

/************************************************************************/
/* MOSTRAR_BITSTR                       05-Sep-13                       */
/* Mostrar los bits del bit string.                                     */
/************************************************************************/
void mostrar_bitstr(void)
{

}

/************************************************************************/
/* MOSTRAR_INGR                         10-Sep-13                       */
/* Mostrar los datos de ingr.                                           */
/************************************************************************/
void mostrar_ingr(void)
{
}

/************************************************************************/
/* NO_ES_ALFANUMERICO                   09-Sep-13                       */
/* Comprueba si el siguiente caracter solo es codificable en ISO/IEC    */
/************************************************************************/
uchar no_es_alfanumerico(uchar c)
{
   if ((c >= 'a'  &&  c <= 'z')  ||  
       (c == '!'  ||  c == '"'  ||  c == '%'  ||  c == '&' ||  c == 39  ||  c == '('  ||  c == ')'  ||  c == '+'  ||
        c == ':'  ||  c == ';'  ||  c == '<'  ||  c == '=' ||  c == '>' ||  c == '?'  ||  c == '_'  ||  c == ' '))
     return(TRUE);
   return(FALSE);
}

/************************************************************************/
/* NUMERIC_ENCODE                       05-Sep-13                       */
/* Codifica los numeros.                                                */
/************************************************************************/
int numeric_encode(uchar *estado, char *s)
{
   int i;
   uint valor,resto,numero_simbolos,length_pData,procesados;
   long padding_bits;

   procesados = 0;
   padding_bits = 0x04210840L; 			/* bits para rellenar cuando no hay mas caracteres. Est�n en orden inverso */
   length_pData = strlen(s);
   if (length_pData >= 2  &&  			/* Tiene que ser digito-digito, digito-FNC1 o FNC1-digito */
      ((s[0] >= '0'  &&  s[0] <= '9')  ||  s[0] == FNC1)  &&  
      ((s[1] >= '0'  &&  s[1] <= '9')  ||  s[1] == FNC1)  &&  
       (s[0] != FNC1  ||  s[1] != FNC1))
     {									/* Convierto de ascii a decimal */
     if (s[0] == FNC1)
       s[0] = 10;
     else
       s[0] -= 0x30;
     if (s[1] == FNC1)
       s[1] = 10;
     else
       s[1] -= 0x30;
     valor = (s[0] * 11) + s[1] + 8;	/* Calculo el valor codificado en 7 bits y lo inserto en el bitString */
     ins_bitstr(valor,7);
     procesados = 2;					/* Dos caracteres procesados */
     *estado = 0;
     }
   else if (length_pData >= 2) 			/* Quedan m�s de dos caracteres pero no se pueden codificar en numerico */
     {
     ins_bitstr(0x00,4);				/* Inserto el Alphanumeric Latch y paso al estado 1 */
     *estado = 1;
     }
   else if (length_pData == 1  &&  (s[0] < '0'  ||  s[0] > '9')) 
     {									/* Si solo queda un caracter y no es un digito */
     ins_bitstr(0x00,4);				/* Inserto el Alphanumeric Latch y paso al estado 1 */
     *estado = 1;
     }
   else if (length_pData == 1  &&  (s[0] >= '0'  ||  s[0] <= '9')) 
     {									/* Si solo queda un caracter y si es un digito */
     resto = 12 - ind_bitstr % 12; 		/*  Bits que restan para llegar a un multiplo de 12 */
     if (resto >= 7  ||  resto < 4)
       {
       s[0] -= 0x30; 					/* Lo paso de ASCII a binario */
       valor = (s[0] * 11) + 10 + 8; 	/* Codifico el digito que queda + un FNC1 (10) */
       ins_bitstr(valor,7);
       }
     else
       {
       valor = s[0] - 0x30;
       valor += 1;
       ins_bitstr(valor,4);
       }
     procesados = 1; 					/* Un caracter procesado */
     *estado = 0;
     }
   else if (length_pData == 0) 			/* No quedan caracteres */
     {
     resto = ind_bitstr % 12;
     if (resto != 0) 					/* Si no hay un numero exacto de caracteres, calculo cuanto hay que a�adir */
       resto = 12 - resto;
     if (stacked == 1)					/* Relleno con los bits de padding */
       {
       /* Calculo cuantos caracteres va a haber */
       numero_simbolos = 1 + ind_bitstr / 12; 	/*  El 1 + es por el check char, que no est� en el bistring */
       numero_simbolos = numero_simbolos + (resto == 0 ? 0 : 1); 	/* Si hab�a resto, habr� un caracter m�s */
       /* Calculo si es necesario padding extra. Si en la ultima linea solo quedar�a un simbolo, es necesario */
       extrapadding = numero_simbolos % row_symbols;  	/* Si el resto es 1, quiere decir que en modo stacked la ultima fila tiene solo un simbolo, y tiene que tener al menos 2 */
       }
     if (extrapadding == 1)
       resto += 12;  					/* A�ado 12 bits m�s de padding */
     for (i=0;i<resto;i++)
       {
       ins_bitstr(padding_bits,1);
       padding_bits = padding_bits >> 1;
       }
     *estado = 3; 						/* Estado final */
     }
  return(procesados);

}

/************************************************************************/
/* PON_BITSTR                           10-Sep-13                       */
/* Escribir un byte en una posicion.                                    */
/************************************************************************/
void pon_bitstr(uchar pos,uchar c)
{
   char	*ptr;

   ptr = (char *)ORG_DATAMATRIX;
   ptr += pos;
   *ptr = c;
}

/************************************************************************/
/* RD_BARRAS_STACKED               		12-Sep-13                       */
/* Lee el dato del buffer barras_stacked.                               */
/************************************************************************/
uchar rd_barras_stacked(int i)
{
   uchar c;
   char	*ptr;

   ptr = (char *)ORG_BARRAS_STACKED;
   c = *(ptr+i);
   return(c);
}

/************************************************************************/
/* RD_CODIGO_BARRAS                		12-Sep-13                       */
/* Lee el dato del buffer codigo_barras.                                */
/************************************************************************/
uchar rd_codigo_barras(int i)
{
   uchar c;
   char	*ptr;

   ptr = (char *)ORG_CODIGO_BARRAS;
   c = *(ptr+i);
   return(c);
}

/************************************************************************/
/* RD_SEPARADORES                  		12-Sep-13                       */
/* Lee un dato del buffer separadores.                                  */
/************************************************************************/
uchar rd_separadores(int i)
{
   uchar c;
   char	*ptr;

   ptr = (char *)ORG_SEPARADORES;
   c = *(ptr+i);
   return(c);
}

/************************************************************************/
/* SEPINF                    			16-Sep-13                       */
/* Escribir el separador inferior.                                      */
/************************************************************************/
void sepinf(int k)
{
   int i,h,v,a,n;

   inistr();                        	/* Reset buffer imagen */
   n = bits_in_row;
   if (k >= num_rows - 2)
     n = bits_in_last_row;
   for (i=bits_in_row*(k+1);i<n*(k+2);i++)
     {
	 if (rd_separadores_stacked(i) == 0)
	   imaean3(0x00,1);
     else
	   imaean3(0xff,1);
	 }
   h = h_pos;
   v = v_pos;
   a = alt;
   alt = 2;
#if CORR_EAN_COMO_LP_2
   dibean(8,E_EXPAND);
#else
   dibean(8,E_128);
#endif
   h_pos = h;
   v_pos = v;
   if (k < num_rows - 1)
     CorreccionPosicionRotacion(alt);
   alt = a;
}

/************************************************************************/
/* SEPMED                    			16-Sep-13                       */
/* Escribir el separador medio.                                         */
/************************************************************************/
void sepmed(int k)
{
   int i,h,v,a,n;

   inistr();                        	/* Reset buffer imagen */
   n = bits_in_row;
   if (k >= num_rows - 2)
     n = bits_in_last_row;
   for (i=0;i<4;i++)
     imaean3(0x00,1);
   for (i=0;i<n-8;i++)
     {
     if (i % 2 == 0)
	   imaean3(0x00,1);
     else
	   imaean3(0xff,1);
	 }
   for (i=0;i<4;i++)
     imaean3(0x00,1);
   h = h_pos;
   v = v_pos;
   a = alt;
   alt = 2;
#if CORR_EAN_COMO_LP_2
   dibean(8,E_EXPAND);
#else  
   dibean(8,E_128);
#endif
   h_pos = h;
   v_pos = v;
   if (k < num_rows - 1)
     CorreccionPosicionRotacion(alt);
   alt = a;
}

/************************************************************************/
/* SEPSUP                    			16-Sep-13                       */
/* Escribir el separador superior.                                      */
/************************************************************************/
void sepsup(int k)
{
   int i,h,v,a,n;

   inistr();                        	/* Reset buffer imagen */
   n = bits_in_row;
   if (k >= num_rows - 2)
     n = bits_in_last_row;
   for (i=bits_in_row*k;i<n*(k+1);i++)
     {
	 if (rd_separadores_stacked(i) == 0)
	   imaean3(0x00,1);
     else
	   imaean3(0xff,1);
	 }
   h = h_pos;
   v = v_pos;
   a = alt;
   alt = 2;
#if CORR_EAN_COMO_LP_2
   dibean(8,E_EXPAND);
#else 
   dibean(8,E_128);
#endif
   h_pos = h;
   v_pos = v;
   if (k < num_rows - 1)
     CorreccionPosicionRotacion(alt);
   alt = a;
}

/************************************************************************/
/* RD_SEPARADORES_STACKED               12-Sep-13                       */
/* Lee un dato del buffer separadores_stacked.                          */
/************************************************************************/
uchar rd_separadores_stacked(int i)
{
   uchar c;
   char	*ptr;

   ptr = (char *)ORG_SEP_STACKED;
   c = *(ptr+i);
   return(c);
}

/************************************************************************/
/* WR_BARRAS_STACKED               		12-Sep-13                       */
/* Escribe en el buffer barras_stacked.                                 */
/************************************************************************/
void wr_barras_stacked(int i,uchar c)
{
   char	*ptr;

   ptr = (char *)ORG_BARRAS_STACKED;
   *(ptr+i) = c;
}

/************************************************************************/
/* WR_CODIGO_BARRAS                		12-Sep-13                       */
/* Escribe un dato en el buffer codigo_barras.                          */
/************************************************************************/
void wr_codigo_barras(int i,uchar c)
{
   char	*ptr;

   ptr = (char *)ORG_CODIGO_BARRAS;
   *(ptr+i) = c;
}

/************************************************************************/
/* WR_SEPARADORES                  		12-Sep-13                       */
/* Escribe un dato en el buffer separadores.                            */
/************************************************************************/
void wr_separadores(int i,uchar c)
{
   char	*ptr;

   ptr = (char *)ORG_SEPARADORES;
   *(ptr+i) = c;
}

/************************************************************************/
/* WR_SEPARADORES_STACKED          		12-Sep-13                       */
/* Escribe un dato en el buffer separadores_stacked.                    */
/************************************************************************/
void wr_separadores_stacked(int i,uchar c)
{
   char	*ptr;

   ptr = (char *)ORG_SEP_STACKED;
   *(ptr+i) = c;
}

//**************************************************************************************
// WR_XMAG  							24-Feb-15                                      *
// Escribir xmag.                                                                      *
//**************************************************************************************
void wr_xmag(int c)
{
   if (c == 0)
   	 c=1;	
   xmag = c;
}

//**************************************************************************************
// RD_XMAG  							12-Dic-14                                      *
// Leer xmag.                                                                          *
//**************************************************************************************
int rd_xmag(void)
{
   return(xmag);
}

//**************************************************************************************
// WR_YMAG  							24-Feb-15                                      *
// Escribir ymag.                                                                      *
//**************************************************************************************
void wr_ymag(int c)
{
   if (c == 0)
   	 c=1;	
   ymag = c;
}

//**************************************************************************************
// RD_YMAG  							12-Dic-14                                      *
// Leer ymag.                                                                          *
//**************************************************************************************
int rd_ymag(void)
{
   return(ymag);
}

//**************************************************************************************
// RD_FONT  							12-Dic-14                                      *
// Leer font.                                                                          *
//**************************************************************************************
int rd_font(void)
{
   return(font);
}

//**************************************************************************************
// RD_FONT  							12-Dic-14                                      *
// Leer font.                                                                          *
//**************************************************************************************
void wr_font(int nFont)
{
   font = nFont;
}

//**************************************************************************************
// RD_ROTACION							12-Dic-14                                      *
// Leer rotacion.                                                                      *
//**************************************************************************************
uchar rd_rotacion(void)
{
   return(rotacion);
}

//**************************************************************************************
// RD_H_POS 							12-Dic-14                                      *
// Leer h_pos.                                                                         *
//**************************************************************************************
int rd_h_pos(void)
{
   return(h_pos);
}

//**************************************************************************************
// WR_H_POS 							12-Dic-14                                      *
// Escribir h_pos.                                                                     *
//**************************************************************************************
void wr_h_pos(int x)
{
   h_pos = x;
}

//**************************************************************************************
// RD_V_POS 							12-Dic-14                                      *
// Leer v_pos.                                                                         *
//**************************************************************************************
int rd_v_pos(void)
{
   return(v_pos);
}

//**************************************************************************************
// WR_V_POS 							12-Dic-14                                      *
// Escribir v_pos.                                                                     *
//**************************************************************************************
void wr_v_pos(int x)
{
   v_pos = x;
}

//**************************************************************************************
// RD_BYTLIN  							12-Dic-14                                      *
// Leer bytlin.                                                                        *
//**************************************************************************************
uchar rd_bytlin(void)
{
   return(bytlin);
}

//**************************************************************************************
// RD_CODEPAGEIMPRESION					12-Dic-14                                      *
// Leer CodepageImpresion.                                                             *
//**************************************************************************************
int rd_CodepageImpresion(void)
{
   return(CodepageImpresion);
}

//**************************************************************************************
// RD_FLG_NEGRITAS     					23-Dic-14                                      *
// Leer flg_negritas.                                                                  *
//**************************************************************************************
uchar rd_flg_negritas(void)
{
   return(flg_negritas);
}

//**************************************************************************************
// RD_ALT     							24-Feb-15                                      *
// Leer alt.                                                                           *
//**************************************************************************************
uchar rd_alt(void)
{
   return(alt);
}

//**************************************************************************************
// RD_DENSIDAD_EAN						24-Feb-15                                      *
// Leer densidad_ean.                                                                  *
//**************************************************************************************
uchar rd_densidad_ean(void)
{
   return(densidad_ean);
}

/************************************************************************/
/* INT128                               29-Ene-10                       */
/* Escribir linea de interpretacion del EAN-128.                        */
/************************************************************************/
static	void int128(uchar n)
{
	static uchar i;
	uchar auxi,x_aux,y_aux,f_aux,rot_aux,j,m;
    uint h,v;
   
    v = 0;
    h = 0;
    m = 0;
    if (flg_datamatrix == TRUE
#if QR_CODE           
        || flg_qr == TRUE
#endif           
        )
      {
  #if DOT_12
      if (densidad_ean != 1)			/* Si tipo de letra > 100, sin interpretacion */
        return;
  #else
      if (densidad_ean != 0)			/* Si tipo de letra > 100, sin interpretacion */
        return;
  #endif
      m = ((n-2) * alt) / 7;			/* Numero de letras que caben en el simbolo */
      switch (rotacion)
        {
        case 0:
          v_pos += alt * n;
          break;
        case 1:
          h_pos -= alt * n;
          break;
        case 2:

          v_pos -= alt * n;
#if (!CORR_DATAMATRIX)
          h_pos += alt * n;
#endif
          break;
        case 3:
#if (!CORR_DATAMATRIX)
          v_pos += alt * n;
#endif     
          h_pos += alt * n;
          break;
        }
      h = h_pos;
      v = v_pos;
      }
	font = F_6X9;
  #if ( FONTS_EN_COMPAQ || TELECARGA_FUENTES )

	if ( _ldfont(font,rotacion) == FALSE )
	{
		font = F_12X17;
		CodepageImpresion = CODEPAGE;
	}
  #endif
    x_aux = xmag;
    y_aux = ymag;
    f_aux = font;
    rot_aux = rotacion;
	xmag = 1;
	ymag = 2;
  #if DOT_12
    if (flg_datamatrix == TRUE
#if QR_CODE           
        || flg_qr == TRUE
#endif           
        )
      {
	  font = F_9X14;
	  ymag = 3;
      }
    else
      {
	  xmag = 2;
	  ymag = 3;
      }
  #endif
	if (rotacion == 0)
		v_pos += alt + 6;
	if (rotacion == 1)
		{
		v_pos += 6;
		h_pos -= (alt + 6);
		}
	if (rotacion == 2)
		v_pos -= (alt + 6);
	if (rotacion == 3)
		{
		v_pos -= 6;
		h_pos += alt + 4;
		}
    if (code128pr)
     auxi = 1;
    else
      auxi = 2;
    if (flg3d9 == TRUE)
      auxi = 0;
    if (flg_datamatrix == TRUE)
      auxi = 0;
#if QR_CODE           
    if (flg_qr == TRUE)
      auxi = 0;
#endif           

    if (flg128 == TRUE)  
	{
		flg128 = FALSE;
		num_digitos_codigo_barras--;
	}    	
    j = 0;
	for (i=auxi;i<num_digitos_codigo_barras;i++)
		{
		/* LC */
		/* Los parentesis que encierran al IA unicamente deben imprimirse en la interpretacion */
		/* humanamente legible */
		/*sprintf(debug,"%c,%c,%c",buf_auxiliar_parser[0],buf_auxiliar_parser[1],buf_auxiliar_parser[2]);
		wridis(0,40,FD6X9,EFENAD,debug);
		sprintf(debug,"%c,%c,%c,%c",buf_auxiliar_parser[3],buf_auxiliar_parser[4]);
		wridis(0,50,FD6X9,EFENAD,debug);*/
		if (code128pr == TRUE)
		  {
		  if (buf_auxiliar_parser[i] < ' ')
			continue;
		  do_letter(buf_auxiliar_parser[i]);
		  }
		else
		  {		
		  if (buf_auxiliar_parser[i] == IA_C)
			buf_auxiliar_parser[i] = '(';
		  if (buf_auxiliar_parser[i] == IA_F)
			buf_auxiliar_parser[i] = ')';
		/* LC */
		  if (buf_auxiliar_parser[i] < ' ')
			continue;
          if (flg_datamatrix == TRUE || flg_qr == TRUE)
            {
		  if (CodepageImpresion == CODEPAGE_850)
		    {
            switch (buf_auxiliar_parser[i])
              {
              case 0xd0:				/* "�" */
			    buf_auxiliar_parser[i] = 0x1c;
                break;
              case 0xd1:				/* "�" */
			    buf_auxiliar_parser[i] = 0x1d;
                break;
              case 0xd2:				/* "�" */
			    buf_auxiliar_parser[i] = 0x1e;
                break;
              case 0xd3:				/* "�" */
			    buf_auxiliar_parser[i] = 0x1f;
                break;
               }
    		 }
            }
		  do_letter(buf_auxiliar_parser[i]);
          if ( (flg_datamatrix == TRUE  
#if QR_CODE           
              || flg_qr == TRUE
#endif           
              ) &&  ++j > m)	/* Partir la linea de interpretacion */
            {
            j = 0;
            switch (rotacion)
              {
              case 0:
                v_pos += 20;
                h_pos = h;
                break;
              case 1:
                h_pos -= 20;
                v_pos = v;
                break;
              case 2:
                v_pos -= 20;
                h_pos = h;
                break;
              case 3:
                h_pos += 20;
                v_pos = v;
                break;
              }
            }
		  }
		}
     font = f_aux;
     xmag = x_aux;
     ymag = y_aux;
     rotacion = rot_aux;
}

/************************************************************************/
/* TSTTIP                               30-Dic-98                       */
/* Mirar si es cter de cambio de tipo en EAN-128.                       */
/************************************************************************/
static	uchar tsttip(uchar c)
{
	/* LC */
	/* 27-9-00 */
	if (c == INICIO_A || c == INICIO_B || c == INICIO_C)
		flag_inicio_ean = TRUE;
	/* LC */
	
	if (c < CAMBIO__)                    /* Cter de cambio de tipo */
		{
		tipcod128 = tbltip[c-INICIO_A];
		return(TRUE);
		}
	if (c == CAMBIO__)                	/* Cter de cambio */
		{
		if (tipcod128 != 2){                 	/* No es tipo de codigo C */
			/* LC */
			/* 27-9-00 */
			if (tipcod128 == 1)
				tipcod128 = 0;                	/* Indicar cambio de codigo */
			else
				tipcod128 = 1;
			/* LC */
			/* 27-9-00 */
			}
		return(TRUE);
		}
	if (c < 30)                       	/* Cter especial */
		return(TRUE);
	return(FALSE);                       /* Cter normal */
}

/************************************************************************/
/* WRI128                               30-Dic-98                       */
/* Escribir el cter en el tipo adecuado. EAN-128.                       */
/************************************************************************/
static	void wri128(uchar c)
{
	static uchar ctr;

	if ( (c == IA_C) || (c == IA_F) )
		return;

	if (tipcod128 == 2)                     /* Tipo C */
		{
		if (flg128 == TRUE)                /* Componer 2� digito */
			{
			flg128 = FALSE;
			ctr += c & 0x0f;                 /* Componer digito */
			chk128(ctr);                     /* Check */
//            SET_BNK_ROM(BNKROM_0);
			imaean((uchar)(tbl128[ctr]>>8),8);
//            SET_BNK_ROM(BNKROM_0);
			imaean((uchar)tbl128[ctr],3);    /* Barras EAN-128 */
			/*wridis(0,30,FD6X9,EFENAD,"TIPCOD1282");*/
			num128++;
			}
		else
			{
			flg128 = TRUE;
			ctr = 10 * (c & 0x0f);           /* Dejar parte baja del cter */
			}
		return;
		}

	/* LC */
	/* 11-08-00 */
	/*   ctr -= ' '; */                          /* Restar offset */
	ctr = c - ' ';
	chk128(ctr);                     	/* Check */
//    SET_BNK_ROM(BNKROM_0);
	imaean((uchar)(tbl128[ctr]>>8),8);   /* Barras EAN-128 */
	/*sprintf(debug,"dib barras:%c",ctr);
	wridis(0,40,FD6X9,EFENAD,debug);*/
//    SET_BNK_ROM(BNKROM_0);
	imaean((uchar)tbl128[ctr],3);
	num128++;
}

/************************************************************************/
/* WRI128E                              30-Dic-98                       */
/* Escribir el cter especial en el tipo adecuado. EAN-128.              */
/************************************************************************/
static	void wri128e(uchar c)
{
	/* LC */
	/* 18-9-00 */
	/* No debe imprimir en el codigo los parentesis que encierran al identificador */
	if ( (c==IA_C) || (c==IA_F) )
		return;
	/* LC */
	
	chk128(tblchk[c-INICIO_A]);          /* Check */
	imaean((uchar)(tbl128e[c-INICIO_A]>>8),8);
	imaean((uchar)tbl128e[c-INICIO_A],3);
	num128++;
}

/************************************************************************/
/* CHK128                               31-Dic-98                       */
/* Calculo del cter. de check del EAN-128.                              */
/************************************************************************/
static	void chk128(uchar c)
{
	uchar n;
	
	n = 1;
	if (code128pr == FALSE)
	{
	if (c != 102)                        /* FNC1 */
		n = ord128++;                      /* Numero de orden */
	else 
      {
		if (flag_inicio_ean == TRUE)
		  flag_inicio_ean = FALSE;
		else
		  n = ord128++;
	  }
	}
	else
	{
	  if (inicio_letra == FALSE)
	   inicio_letra = TRUE;
	  else
	  {
	   if (inicio_letra == TRUE)
	     n = ord128 ++;
	  }
	}
	sum128 += (long) (n * c);           	/* Peso cter = valor * num de orden */
}

 #if OFFSET_ETIQUETA
//***********************************************************************/
// RD_OFFSET                            18-Abr-16                       */
// Leer el offset.                                                      */
//***********************************************************************/
int rd_offset(void)
{
   uchar s;
   int n;

   s = get_ctr_parser();
   n = rd_int2_buffer_parser();     	/* Leer un entero de 4 bytes */
   if (s == '-')
     n = -n;
   return(n);
}
  #endif

#if QR_CODE

//***********************************************************************/
// DUP_QR                                            29-Dic-17          */
// Pasar de la imagen al string del qr.                                 */
//***********************************************************************/
void dup_qr(uchar fil, uchar t, uchar n, uchar *bm)
{
   uchar a,i,c,m,j,k,d,mj,jj;
   uchar sizeRowBytes;
   char	*ptr;

   sizeRowBytes = ((n + (8 - (n % 8))) / 8);
   memset(imastr,0x00,200);

   a = t + 3;
   ptr = (char *)bm;
   ptr += fil * sizeRowBytes;
   jj = 0;
   mj = 0x80;
   for (i=0;i<sizeRowBytes;i++)
     {
     m = 0x80;							/* Mascara */
     d = *ptr++;						/* Dato leido */
     for (j=0;j<8;j++)
       {
       c = d & m;						/* bit leido */
       m = m >> 1;
       for (k=0;k<a;k++)				/* Repetir el bit n veces */
         {
         if (c != 0x00  &&  jj < 200)
           imastr[jj] |= mj;
         mj = mj >> 1;
         if (mj == 0x00)
           {
           mj = 0x80;
           jj++;
           }
         }
       }
     }
}

//***********************************************************************/
// DIB_QR                               01-Dic-17                       */
// Dibujar codigo QR                                                    */
//************************* *********************************************/
uchar 	dib_qr(void)
{
    uchar i, n, tam, sizeRowBytes;
    char aux[4];
    struct QRcode *qrcod = (struct QRcode *)obtenQRcode();
    
#if DOT_12
    alt = ((alt * 8) + 6) / 12;
#endif
    for (i = 0; i<MAXALTEAN; i++)			/* Con la altura del EAN calcular la densidad */
    {
        memmove(aux, &tblalt[i][2], 3);
        aux[3] = 0x00;
        if (alt <= (uchar)atoi(aux))
            break;
    }
    tam = i;
#if DOT_12
    tam = ((tam * 12) + 4) / 8;
#endif
    n = cal_qr();

    
    if (n == 0)
        return(0);

    sizeRowBytes = ((n + (8 - (n % 8))) / 8);
    indstr = (sizeRowBytes + 1) * (tam + 3);

    
    for (i = 0; i<n; i++)
    {
        dup_qr(i, tam, n, &qrcod->codigo.m[0]);
        lin_datamatrix(tam);				/* Dibujar cada linea del qr, se usa la misma funcion que el datamatrix */
    }

    if (n > 25)     /* Si la version es > 2 truncamos para que no imprima la l�nea de interpretacion puesto que ser� muy larga */
        n = 0;
    return(n);
}

//***********************************************************************/
// obtenBufParser                               16-Abr-19               */
// uchar  *obtenBufParser(void)                                         */
// **********************************************************************/
uchar 	*obtenBufParser (void)
{
	return &buf_auxiliar_parser[1]; /* el +1 al buffer es por el caracter FNC1 que hay al comienzo */
}

//***********************************************************************/
// obtenNumDig                               16-Abr-19                  */
// uint    obtenNumDig(void)                                            */
//**************************************** ******************************/
uint 	obtenNumDig (void)
{
	return (num_digitos_codigo_barras - (1)); /* el -1 a la longitud es por el caracter FNC1 que hay al comienzo */
}

#endif
