/* pmmixer.h -- defines for the PMMIXER app */

#define ID_MIXER 0x0001
#define IDM_MIXERDLG 0x1000

#define ID_MASTERLEFT              101
#define ID_MASTERRIGHT             102
#define ID_VOCLEFT                 103
#define ID_VOCRIGHT                104
#define ID_FMLEFT                  105
#define ID_FMRIGHT                 106
#define ID_LINELEFT                107
#define ID_LINERIGHT               108
#define ID_CDLEFT		   109
#define ID_CDRIGHT                 110
#define ID_MIC                     111

#define ID_RECORDMIC               120
#define ID_RECORDLINE              121
#define ID_RECORDCD                122

#define ID_FILTERINHIGH            130
#define ID_FILTERINLOW             131
#define ID_FILTERINOFF             132
#define ID_FILTEROUTON             133
#define ID_FILTEROUTOFF            134

#define ID_LOCKVOL                 140
#define ID_MUTE                    141
#define ID_STEREO                  142

#define ID_VOCTEXT                 150
#define ID_FMTEXT                  151
#define ID_LINETEXT                152
#define ID_CDTEXT                  153
#define ID_MICTEXT                 154
#define ID_MASTERTEXT              155
#define ID_SOURCEBOX               156
#define ID_FILTERBOX               157
#define ID_MIXERBOX                158
#define ID_INPUTTEXT               159
#define ID_OUTPUTTEXT              160

#define ID_MIXERICO		   170

#define ID_MENUSEPARATOR           180
#define ID_ABOUTMENU               181
#define ID_HELPMENU                182
#define ID_AGRESSIVEMENU           183

#define ID_BIGMIXER                190
#define ID_ABOUTDLG                200

#define SB_BASEADDRESS 0x220    /* SB base address -- this should be
                                   replaced by a custom control */

#define SB_MIXER_ADDR SB_BASEADDRESS+0x04
#define SB_MIXER_DATA SB_BASEADDRESS+0x05

/* SB mixer registers are shared. REC_SRC and IN_FILTER use
   the same port, and CHANNELS and OUT_FILTER another. */

#define RECORD_SRC      0x0c    /* ADC input selection reg */
#define IN_FILTER       0x0c    /* "ANFI" input filter reg */
/* bit:   7 6 5 4 3 2 1 0       F=frequency (0=low, 1=high)
          x x T x F S S x       SS=source (00=MIC, 01=CD, 11=LINE yes 11)
                                T=input filter switch */

#define CHANNELS        0x0e    /* Stereo/Mono output select */
#define OUT_FILTER      0x0e    /* "DNFI" output filter reg */
/* bit:   7 6 5 4 3 2 1 0       F=frequency (0=low, 1=high)
          x x T x F x S x       S=stereo (0=mono, 1=stereo)
                                T=input filter switch */

#define FREQ_HI         (1<<3)  /* Use High-frequency filters */
#define FREQ_LOW        0       /* Use Low-frequency filters */
#define FILT_ON         0       /* Yes, 0 to turn it on, 1 for off */
#define FILT_OFF        (1<<5)
#define FILT_HIGH       FILT_ON+FREQ_HI
#define FILT_LOW        FILT_ON+FREQ_LOW

/* These three defines are only for internal use of the mixer app */
#define HIGH 2
#define LOW  1
#define ON   1
#define OFF  0

#define MONO_DAC        0       /* Send to CHANNELS for mono output */
#define STEREO_DAC      2       /* Send to CHANNELS for stereo output */

#define VOL_MASTER      0x22    /* High nibble is left, low is right */
#define VOL_VOC         0x04
#define VOL_FM          0x26
#define VOL_LINE        0x2e
#define VOL_CD          0x28
#define VOL_MIC         0x0a    /* Only the lowest three bits (0-7) */

