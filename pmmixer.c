/*	pmmixer.c

        Sound Blaster Pro Mixer for PM, version 2.0.

	Version 1.0 originally written by David Nichols
	for use with the SBOS2.SYS driver by Michael Fulbright.

        Version 1.2 rewritten for Borland C++, scrollbar controls
        replaced by sliders and various other enhancements
        by Jeroen Hoppenbrouwers.

        Version 2.0 rewritten to access the SB Pro hardware directly
        in order to function under OS/2 2.1's MMPM/2. This triggered
        a major overhaul of the interface as well. A real MMPM/2
        version is in the works.
*/

#define INCL_DOS
#define INCL_WIN

#include <os2.h>
#include "pmmixer.h"
#include "sblast_user.h"
#include "portio.h"

#define NUMSLIDERS 11

extern int _argc;
extern char * _argv[];

/* We need direct hardware port I/O. These routines provide us with it */

unsigned short _cdecl _far16 inp(unsigned short port);
unsigned short _cdecl _far16 outp(unsigned short port,
                                  unsigned short byte);

/*This is defined because the definition for SWCNTRL from GCC/2 is incorrect */
typedef struct _REAL_SWCNTRL
{
   HWND     hwnd;
   HWND     hwndIcon;
   HPROGRAM hprog;
   PID      idProcess;
   ULONG    idSession;
   ULONG    uchVisibility;
   ULONG    fbJump;
   CHAR     szSwtitle[MAXNAMEL+4];
   ULONG    bProgType;
} REAL_SWCNTRL;


struct volumesliderrec
/* This stuct holds values OF THE SB--not of the sliders!
   E.g., the MasterLeft volume ranges from 1 to 15 in steps of 2 */
{
   ULONG id;
   SHORT current;
};

struct volumesliderrec sliders[NUMSLIDERS] =
{
  ID_MASTERLEFT, 1,     ID_MASTERRIGHT, 1,
  ID_VOCLEFT,    3,     ID_VOCRIGHT,    3,
  ID_FMLEFT,     5,     ID_FMRIGHT,     5,
  ID_LINELEFT,   7,     ID_LINERIGHT,   7,
  ID_CDLEFT,     9,     ID_CDRIGHT,     9,
  ID_MIC,        3
  };

HFILE mixer_handle;
HWND hwndMixer = NULL;
HAB hab;

char title[]  = "Mixer";

int recsrc;
int filterin;
int filterout;
int mute = 0;       /* This setting is local to the mixer (no SB equiv) */
int stereo;
int locklr = 1;     /* This setting is local to the mixer (no SB equiv) */
int agressive = 0;  /* This setting is local to the mixer (no SB equiv) */

int init_in_progress = 1;

/* This function reads a specific SB Pro mixer register */
SHORT readSBreg(int SBmixerChannel)
{
   outp(SB_MIXER_ADDR, SBmixerChannel);
   return inp(SB_MIXER_DATA);
}

/* This function sets a specific SB Pro mixer register */
void setSBreg(int SBmixerChannel, SHORT mixer_data)
{
   outp(SB_MIXER_ADDR, SBmixerChannel);
   outp(SB_MIXER_DATA, mixer_data);
   return;
}

void ReadSBLevels()
{
   SHORT data;

   if (!mute)
   {
      data = readSBreg(VOL_MASTER);
      sliders[0].current = ((data >> 4) & 0xF);
      sliders[1].current = (data & 0xF);
   }
   data = readSBreg(VOL_VOC);
   sliders[2].current  = ((data >> 4) & 0xF);
   sliders[3].current  = (data & 0xF);
   data = readSBreg(VOL_FM);
   sliders[4].current  = ((data >> 4) & 0xF);
   sliders[5].current  = (data & 0xF);
   data = readSBreg(VOL_LINE);
   sliders[6].current  = ((data >> 4) & 0xF);
   sliders[7].current  = (data & 0xF);
   data = readSBreg(VOL_CD);
   sliders[8].current  = ((data >> 4) & 0xF);
   sliders[9].current  = (data & 0xF);
   data = readSBreg(VOL_MIC);
   sliders[10].current = (data & 0x7);
}

void SetSBLevels()
{
   if (mute)
     setSBreg(VOL_MASTER, (1<<4)+1);
   else
     setSBreg(VOL_MASTER, (sliders[0].current<<4)+sliders[1].current);

   setSBreg(VOL_VOC, (sliders[2].current<<4)+sliders[3].current);
   setSBreg(VOL_FM,  (sliders[4].current<<4)+sliders[5].current);
   setSBreg(VOL_LINE,(sliders[6].current<<4)+sliders[7].current);
   setSBreg(VOL_CD,  (sliders[8].current<<4)+sliders[9].current);
   setSBreg(VOL_MIC,  sliders[10].current);
}


/* The SB control registers overlap. We have to combine both the
   recording source and the input filter into one register,
   and the stereo switch and the output filter in another */
void SetSBParams()
{
   SHORT data;

   data = recsrc<<1;
   switch (filterin)
   {
      case OFF:
        data += FILT_OFF;
        break;
      case LOW:
        data += FILT_LOW;
        break;
      case HIGH:
        data += FILT_HIGH;
        break;
   }
   setSBreg(IN_FILTER,data);

   switch (filterout)
   {
      case ON:
        data = FILT_ON;
        break;
      case OFF:
        data = FILT_OFF;
        break;
   }
   if (stereo)
     data += STEREO_DAC;
   setSBreg(OUT_FILTER,data);
}

void ReadSBParams()
{
   SHORT data;

   data = readSBreg(RECORD_SRC);
   recsrc = ((data >> 1) & 0x3);

   data = readSBreg(IN_FILTER);
   if (data & FILT_OFF)
     filterin = OFF;
   else
     {
       if (data & FREQ_HI)
         filterin = HIGH;
       else
         filterin = LOW;
     }

   data = readSBreg(OUT_FILTER);
   if (data & FILT_OFF)
     filterout = OFF;
   else
     filterout = ON;

   data = readSBreg(CHANNELS);
   if (data & STEREO_DAC)
     stereo = 1;
   else
     stereo = 0;
}


void UpdateButtons(HWND hwnd)
{
HWND hwndSysMenu;
HWND hwndSysSubMenu;
ULONG itemID;
MENUITEM menuitem;

WinSendDlgItemMsg(hwnd, ID_RECORDLINE, BM_SETCHECK, (MPARAM)(recsrc==SRC_LINE), (MPARAM)0);
WinSendDlgItemMsg(hwnd, ID_RECORDMIC,  BM_SETCHECK, (MPARAM)(recsrc==SRC_MIC),  (MPARAM)0);
WinSendDlgItemMsg(hwnd, ID_RECORDCD,   BM_SETCHECK, (MPARAM)(recsrc==SRC_CD),   (MPARAM)0);

WinSendDlgItemMsg(hwnd, ID_FILTERINHIGH, BM_SETCHECK, (MPARAM)(filterin==HIGH), (MPARAM)0);
WinSendDlgItemMsg(hwnd, ID_FILTERINLOW,  BM_SETCHECK, (MPARAM)(filterin==LOW),  (MPARAM)0);
WinSendDlgItemMsg(hwnd, ID_FILTERINOFF,  BM_SETCHECK, (MPARAM)(filterin==OFF),  (MPARAM)0);

WinSendDlgItemMsg(hwnd, ID_FILTEROUTON,  BM_SETCHECK, (MPARAM)(filterout==ON),  (MPARAM)0);
WinSendDlgItemMsg(hwnd, ID_FILTEROUTOFF, BM_SETCHECK, (MPARAM)(filterout==OFF), (MPARAM)0);

WinSendDlgItemMsg(hwnd, ID_STEREO,  BM_SETCHECK, (MPARAM)(stereo), (MPARAM)0);

WinSendDlgItemMsg(hwnd, ID_MUTE,    BM_SETCHECK, (MPARAM)(mute),   (MPARAM)0);
WinSendDlgItemMsg(hwnd, ID_LOCKVOL, BM_SETCHECK, (MPARAM)(locklr), (MPARAM)0);

/* Get a handle on the system (sub)menu */
hwndSysMenu = WinWindowFromID(hwnd, FID_SYSMENU);
itemID = WinSendMsg(hwndSysMenu, MM_ITEMIDFROMPOSITION,
                   MPFROMSHORT(0),  /* first menu position */
                   NULL);
WinSendMsg(hwndSysMenu, MM_QUERYITEM,
          MPFROM2SHORT(itemID,TRUE),
          (MPARAM)&menuitem);
hwndSysSubMenu = menuitem.hwndSubMenu;
if (agressive)
  WinSendMsg(hwndSysSubMenu, MM_SETITEMATTR,
             (MPARAM)ID_AGRESSIVEMENU,
             MPFROM2SHORT(MIA_CHECKED,MIA_CHECKED));
else
  WinSendMsg(hwndSysSubMenu, MM_SETITEMATTR,
             (MPARAM)ID_AGRESSIVEMENU,
             MPFROM2SHORT(MIA_CHECKED,0));
}

void UpdateSliderPos(HWND hwnd)
/* It is absolutely NECESSARY to check sliders for their current
   position before updating. If not, we will end up in an endless
   loop, with the left slider updating the right etc. etc. etc. */
{
   int i;
   USHORT sliderpos;
   for(i=0;i<NUMSLIDERS;i++)
   {
     sliderpos = (ULONG)WinSendDlgItemMsg(hwnd, sliders[i].id,
                  SLM_QUERYSLIDERINFO,
                  MPFROM2SHORT(SMA_SLIDERARMPOSITION,
		               SMA_INCREMENTVALUE),
                  NULL);
     if (sliderpos!=(sliders[i].current-1)/2)
     {
       WinSendDlgItemMsg(hwnd, sliders[i].id,
       SLM_SETSLIDERINFO,
       MPFROM2SHORT(SMA_SLIDERARMPOSITION,SMA_INCREMENTVALUE),
       MPFROMSHORT( (sliders[i].current-1)/2) );
     }
   }

}

void StoreNewMixerLevel(ULONG id, ULONG newpos)
{
   ULONG i = 0;
   while (((int)i<NUMSLIDERS)&&(id!=sliders[i].id)) i++;
   if (i<NUMSLIDERS)
   {
      sliders[i].current = (newpos*2)+1;
      if (locklr&&(i<(NUMSLIDERS-1)))
      {
	 if (!(i%2)) i++;
	 else i--;
	 sliders[i].current = (newpos*2)+1;
      }
   }
}

void LockVolume(HWND hwnd)
{
   int i;
   for(i=0;i<NUMSLIDERS-1;i+=2)
      sliders[i].current = sliders[i+1].current;
   SetSBLevels();
   UpdateSliderPos(hwnd);
}

void InitSliders(HWND hwnd)
{
   int i;
   for(i=0;i<NUMSLIDERS;i++)
   {
     WinSendDlgItemMsg(hwnd, sliders[i].id,
       SLM_SETSLIDERINFO,
       MPFROMSHORT(SMA_SLIDERARMDIMENSIONS),
       MPFROM2SHORT(8,16));

     WinSendDlgItemMsg(hwnd, sliders[i].id,
       SLM_SETSLIDERINFO,
       MPFROMSHORT(SMA_SHAFTDIMENSIONS),
       MPFROMSHORT(6));

     WinSendDlgItemMsg(hwnd, sliders[i].id,
       SLM_SETTICKSIZE,
       MPFROM2SHORT(SMA_SETALLTICKS,10),
       NULL);
   }
}


MRESULT EXPENTRY AboutDlgProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)

/* When this dialog box is opened, start up a timer that triggers
   about every 150 ms and posts a message to this window.
   React to that message by moving the MASTER_LEFT slider one position
   upwards, until the top is reached, then downwards etc.
   If the OK button is pressed, kill the timer and return. */
{

HWND  hwndParent;

int slider;

struct sliderpos
{
   SHORT position;
   SHORT direction;
};

static struct sliderpos Sliders[NUMSLIDERS] =
{
  3, 1,
  2, 1,
  1, 0,
  2, 0,
  3, 0,
  4, 0,
  5, 0,
  6, 0,
  7, 0,
  6, 1,
  5, 1
};

   switch(msg)
   {
      case WM_INITDLG:
        WinStartTimer(hab, hwnd, 1, 150);
        break;

      case WM_TIMER:
        hwndParent = WinQueryWindow(hwnd, QW_OWNER);

        for(slider=0;slider<NUMSLIDERS;slider++)
        {
          if (slider!=NUMSLIDERS-1)
            WinSendDlgItemMsg(hwndParent, sliders[slider].id,
              SLM_SETSLIDERINFO,
              MPFROM2SHORT(SMA_SLIDERARMPOSITION,SMA_INCREMENTVALUE),
              MPFROMSHORT(Sliders[slider].position) );
          else
            WinSendDlgItemMsg(hwndParent, sliders[slider].id,
              SLM_SETSLIDERINFO,
              MPFROM2SHORT(SMA_SLIDERARMPOSITION,SMA_INCREMENTVALUE),
              MPFROMSHORT(Sliders[slider].position/2) );

          if (Sliders[slider].direction)
            Sliders[slider].position += 1;
          else
            Sliders[slider].position -= 1;

          if ((Sliders[slider].position>6) ||
              (Sliders[slider].position<1)   )
            Sliders[slider].direction = !Sliders[slider].direction;
        }

        break;

      case WM_COMMAND:
        switch (SHORT1FROMMP(mp1))
        {
          case DID_OK:
            WinStopTimer(hab, hwnd, 1);
            WinDismissDlg(hwnd, DID_OK);
            return 0;
        }
        break;
   }
   return WinDefDlgProc(hwnd, msg, mp1, mp2);
}


MRESULT EXPENTRY MixerDlgProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   static USHORT id, mess;
   static USHORT sliderpos;
   static int aboutbox_runs = 0;
   static int firsttime = 1;
   PSWP pswp;
   ULONG i = 0;
   HWND hwndSysMenu;
   ULONG itemID;
   static HWND hwndSysSubMenu;
   PSZ pszMenuText;
   MENUITEM menuitem;
   HWND hwndItem;
   static HINI hini;
   ULONG cb;
   SWP swp;
   ULONG x,y,fl;

   switch(msg)
   {
      case WM_INITDLG:
         init_in_progress = 1;  /* To prevent circular updates */

	 /* Get a handle on the system (sub)menu */
	 hwndSysMenu = WinWindowFromID(hwnd, FID_SYSMENU);
	 itemID = WinSendMsg(hwndSysMenu, MM_ITEMIDFROMPOSITION,
			     MPFROMSHORT(0),  /* first menu position */
			     NULL);
	 WinSendMsg(hwndSysMenu, MM_QUERYITEM,
		    MPFROM2SHORT(itemID,TRUE),
		    (MPARAM)&menuitem);
	 hwndSysSubMenu = menuitem.hwndSubMenu;

	 /* Remove some standard system (sub)menu items */
	 WinSendMsg(hwndSysSubMenu, MM_REMOVEITEM,
		    MPFROM2SHORT(SC_MAXIMIZE, FALSE),
		    NULL);
	 WinSendMsg(hwndSysSubMenu, MM_REMOVEITEM,
		    MPFROM2SHORT(SC_SIZE, FALSE),
		    NULL);
	 WinSendMsg(hwndSysSubMenu, MM_REMOVEITEM,
		    MPFROM2SHORT(SC_HIDE, FALSE),
		    NULL);

	 /* Add items to the sys(sub)menu */
	 pszMenuText          = "";
	 menuitem.iPosition   = MIT_END;
	 menuitem.afStyle     = MIS_SEPARATOR;
	 menuitem.afAttribute = 0;
	 menuitem.id          = ID_MENUSEPARATOR;
	 menuitem.hwndSubMenu = NULL;
	 menuitem.hItem       = 0;
	 WinSendMsg(hwndSysSubMenu, MM_INSERTITEM,
		      (MPARAM)&menuitem, (MPARAM)pszMenuText);

	 pszMenuText          = "Ag~ressive";
	 menuitem.iPosition   = MIT_END;
	 menuitem.afStyle     = MIS_TEXT;
	 menuitem.afAttribute = 0;
	 menuitem.id          = ID_AGRESSIVEMENU;
	 menuitem.hwndSubMenu = NULL;
	 menuitem.hItem       = 0;
	 WinSendMsg(hwndSysSubMenu, MM_INSERTITEM,
		      (MPARAM)&menuitem, (MPARAM)pszMenuText);

	 pszMenuText          = "~About Mixer...";
	 menuitem.iPosition   = MIT_END;
	 menuitem.afStyle     = MIS_TEXT;
	 menuitem.afAttribute = 0;
	 menuitem.id          = ID_ABOUTMENU;
	 menuitem.hwndSubMenu = NULL;
	 menuitem.hItem       = 0;
	 WinSendMsg(hwndSysSubMenu, MM_INSERTITEM,
		      (MPARAM)&menuitem, (MPARAM)pszMenuText);

	 pszMenuText          = "~Help...";
	 menuitem.iPosition   = MIT_END;
	 menuitem.afStyle     = MIS_HELP;
	 menuitem.afAttribute = 0;
	 menuitem.id          = 0; /*ID_HELPMENU*/;
	 menuitem.hwndSubMenu = NULL;
	 menuitem.hItem       = 0;
	 WinSendMsg(hwndSysSubMenu, MM_INSERTITEM,
		      (MPARAM)&menuitem, (MPARAM)pszMenuText);

	 /* Set up slider shafts etc. */
	 InitSliders(hwnd);

         /* Read in the .ini file and set the mixer accordingly */
         hini = PrfOpenProfile(hab,"pmmixer.ini");
         PrfQueryProfileSize(hini,"PMmixer","Sliders",&cb);
         if (cb==sizeof(sliders))   /* correct .ini available? */
           {
            PrfQueryProfileData(hini,"PMmixer","Sliders",&sliders,&cb);
            setSBlevels();

            cb = sizeof(recsrc);
            PrfQueryProfileData(hini,"PMmixer","RecSrc",&recsrc,&cb);
            cb = sizeof(filterin);
            PrfQueryProfileData(hini,"PMmixer","FilterIn",&filterin,&cb);
            cb = sizeof(filterout);
            PrfQueryProfileData(hini,"PMmixer","FilterOut",&filterout,&cb);
            cb = sizeof(stereo);
            PrfQueryProfileData(hini,"PMmixer","Stereo",&stereo,&cb);
            setSBparams();

            cb = sizeof(locklr);
            PrfQueryProfileData(hini,"PMmixer","LockLR",&locklr,&cb);
            cb = sizeof(agressive);
            PrfQueryProfileData(hini,"PMmixer","Agressive",&agressive,&cb);
            if (agressive)
              WinStartTimer(hab, hwnd, 2, 100);
            cb = sizeof(mute);
            PrfQueryProfileData(hini,"PMmixer","Mute",&mute,&cb);
            if (mute)
              setSBreg(VOL_MASTER, (1<<4)+1);

            /* For some reason the icon will not paint properly if the
               thing comes up minimized. With an ugly trick we force it
               to paint */
            cb = sizeof(x);
            PrfQueryProfileData(hini,"PMmixer","X-pos",&x,&cb);
            cb = sizeof(y);
            PrfQueryProfileData(hini,"PMmixer","Y-pos",&y,&cb);
            WinSetWindowPos(hwnd,HWND_TOP,x,y,0,0,SWP_MOVE);
            /* Now it always comes up restored; if we need it minimized,
               just press the Minimize button: */
            cb = sizeof(fl);
            PrfQueryProfileData(hini,"PMmixer","Minimized",&fl,&cb);
            if (fl==SWP_MINIMIZE)
               WinPostMsg(hwnd,WM_SYSCOMMAND,(MPARAM)SC_MINIMIZE,NULL);
           }
         else
           {
             WinMessageBox(HWND_DESKTOP, hwnd,
                 "No ini file (PMMIXER.INI) found, it has been created in the default directory.",
                 "Mixer Message", 0,
                 MB_INFORMATION|MB_MOVEABLE|MB_OK);
            /* Read the current SB settings and adjust the mixer to them */
            ReadSBParams();
            ReadSBLevels();
           };

/* Add here command line parameter parsing */

         UpdateButtons(hwnd);
         UpdateSliderPos(hwnd);

         init_in_progress = 0;
	 break;

      case WM_MINMAXFRAME:
         /* Ugly function. Dialog boxes are usually not minimized. This one is.
            PM does not stop the controls from being drawn, so they will display
            right over the icon if we don't stop them. We have to disable all
            controls in the lower-left corner of the dialog box. */
         pswp = mp1;
         if (pswp->fl & SWP_MINIMIZE)
           {
            hwndItem = WinWindowFromID(hwnd,ID_VOCLEFT);
	    WinShowWindow(hwndItem, FALSE);
            hwndItem = WinWindowFromID(hwnd,ID_VOCRIGHT);
	    WinShowWindow(hwndItem, FALSE);
           }
         else
           {
            hwndItem = WinWindowFromID(hwnd,ID_VOCLEFT);
	    WinShowWindow(hwndItem, TRUE);
            hwndItem = WinWindowFromID(hwnd,ID_VOCRIGHT);
	    WinShowWindow(hwndItem, TRUE);
           };
	 break;

      case WM_ACTIVATE:
  	 if (!init_in_progress && SHORT1FROMMP(mp1))
           {
            ReadSBParams();
            ReadSBLevels();
            UpdateButtons(hwnd);
            UpdateSliderPos(hwnd);
           };
	 break;

      case WM_TIMER:
         setSBlevels();
         setSBparams();
         break;

      case WM_CONTROL:
         if (init_in_progress)  /* Avoid controls updating others
                                   before being initialised properly */
           break;

	 id   = SHORT1FROMMP(mp1);
	 mess = SHORT2FROMMP(mp1);

         switch (id)
           {
            case ID_MASTERLEFT:
            case ID_MASTERRIGHT:
            case ID_VOCLEFT:
            case ID_VOCRIGHT:
            case ID_FMLEFT:
            case ID_FMRIGHT:
            case ID_LINELEFT:
            case ID_LINERIGHT:
            case ID_CDLEFT:
            case ID_CDRIGHT:
            case ID_MIC:
              if (aboutbox_runs)
                break;
              if ((mess==SLN_CHANGE) || (mess==SLN_SLIDERTRACK))
                {
                 /* These messages are unfit for our purpose, so we
                    have to translate them into slider positions first */
		 sliderpos = (ULONG)WinSendDlgItemMsg(hwnd, id,
		              SLM_QUERYSLIDERINFO,
                              MPFROM2SHORT(SMA_SLIDERARMPOSITION,
				           SMA_INCREMENTVALUE),
		              NULL);
	         StoreNewMixerLevel(id, sliderpos);
	         SetSBLevels();
	         UpdateSliderPos(hwnd);
                };
              break;

            case ID_RECORDLINE:
              if (firsttime)   /* Don't ask me why this is necessary */
                {
                 firsttime = 0;
                 UpdateButtons(hwnd);
                 break;
                }
              recsrc = SRC_LINE;
              SetSBParams();
              UpdateButtons(hwnd);
              break;

            case ID_RECORDCD:
              recsrc = SRC_CD;
              SetSBParams();
              UpdateButtons(hwnd);
              break;

            case ID_RECORDMIC:
              recsrc = SRC_MIC;
              SetSBParams();
              UpdateButtons(hwnd);
              break;

            case ID_FILTERINHIGH:
              filterin = HIGH;
              SetSBParams();
              UpdateButtons(hwnd);
              break;

            case ID_FILTERINLOW:
              filterin = LOW;
              SetSBParams();
              UpdateButtons(hwnd);
              break;

            case ID_FILTERINOFF:
              filterin = OFF;
              SetSBParams();
              UpdateButtons(hwnd);
              break;

            case ID_FILTEROUTON:
              filterout = ON;
              SetSBParams();
              UpdateButtons(hwnd);
              break;

            case ID_FILTEROUTOFF:
              filterout = OFF;
              SetSBParams();
              UpdateButtons(hwnd);
              break;

            case ID_MUTE:
              mute = !mute;
              SetSBLevels();
              UpdateButtons(hwnd);
              break;

            case ID_LOCKVOL:
              locklr = !locklr;
              if (locklr) LockVolume(hwnd);
              UpdateButtons(hwnd);
              break;

            case ID_STEREO:
              stereo = !stereo;
              SetSBParams();
              UpdateButtons(hwnd);
              break;
           };
         break;

      /* When the process is terminated through the system menu, we
         receive a WM_CLOSE message; but all other terminations (OS/2 kill,
         Window List) cause only a WM_DESTROY */
      case WM_CLOSE:
         return WinDefWindowProc(hwnd, msg, mp1, mp2); /* triggers WM_DESTROY */

      case WM_DESTROY:
           if (agressive)
             WinStopTimer(hab, hwnd, 2);

           PrfWriteProfileData(hini, "PMmixer", "Sliders",
                               &sliders, sizeof(sliders));
           PrfWriteProfileData(hini, "PMmixer", "RecSrc",
                               &recsrc, sizeof(recsrc));
           PrfWriteProfileData(hini, "PMmixer", "FilterIn",
                               &filterin, sizeof(filterin));
           PrfWriteProfileData(hini, "PMmixer", "FilterOut",
                               &filterout, sizeof(filterout));
           PrfWriteProfileData(hini, "PMmixer", "Mute",
                               &mute, sizeof(mute));
           PrfWriteProfileData(hini, "PMmixer", "Stereo",
                               &stereo, sizeof(stereo));
           PrfWriteProfileData(hini, "PMmixer", "LockLR",
                               &locklr, sizeof(locklr));
           PrfWriteProfileData(hini, "PMmixer", "Agressive",
                               &agressive, sizeof(agressive));

           WinQueryWindowPos(hwnd,&swp);
           fl = (swp.fl & SWP_MINIMIZE);
           PrfWriteProfileData(hini, "PMmixer", "Minimized",
                               &fl, sizeof(fl));
           /* Now restore the window, else we might get the ICON position.
              Because WM_DESTROY was received, the window is already
              made invisible, so restoring it doesn't screw up the screen */
           WinSendMsg(hwnd,WM_SYSCOMMAND,MPFROMSHORT(SC_RESTORE),NULL);
           WinQueryWindowPos(hwnd,&swp);
           PrfWriteProfileData(hini, "PMmixer", "X-pos",
                               &swp.x, sizeof(swp.x));
           PrfWriteProfileData(hini, "PMmixer", "Y-pos",
                               &swp.y, sizeof(swp.y));

           PrfCloseProfile(hini);

	   return WinDefWindowProc(hwnd, msg, mp1, mp2);
           break;

      case WM_COMMAND:
	 switch(SHORT1FROMMP(mp1))
	 {
	    case ID_ABOUTMENU:
               aboutbox_runs = 1;
               WinDlgBox(HWND_DESKTOP,hwnd, AboutDlgProc,
                         NULLHANDLE, ID_ABOUTDLG, NULL);
               aboutbox_runs = 0;
	       return (0);
	       break;

            case ID_AGRESSIVEMENU:
               agressive = !agressive;
               if (agressive)
                 {
                  WinSendMsg(hwndSysSubMenu, MM_SETITEMATTR,
                             (MPARAM)ID_AGRESSIVEMENU,
                             MPFROM2SHORT(MIA_CHECKED,MIA_CHECKED));
                  WinStartTimer(hab, hwnd, 2, 100);
                 }
               else
                 {
                  WinSendMsg(hwndSysSubMenu, MM_SETITEMATTR,
                             (MPARAM)ID_AGRESSIVEMENU,
                             MPFROM2SHORT(MIA_CHECKED,0));
                  WinStopTimer(hab, hwnd, 2);
                 }

               return (0);
               break;
	 }

      case WM_HELP:
	 WinMessageBox(HWND_DESKTOP, hwnd,
"This is still a beta version of PMMIXER.\n\n\
I refuse to call it out of beta until Creative Labs provides \
us with REAL SoundBlaster OS/2 drivers. Look at the PAS-16 \
support, guys!\n\n\
Use of this program is free, but if you like it, please call \
Creative Labs at (int+)1-405-742-6622 and complain about the current lack \
of decent OS/2 drivers and APIs for their products.\n\n\
The new things in this release are mainly the persistent \
settings: even the position on the screen is retained.\n\n\
Please mail any bug reports or suggestions to hoppie@kub.nl.\n\n",
		       "Help for Mixer", 0,
		       MB_INFORMATION|MB_MOVEABLE|MB_OK);
	 break;
      break;
   }
   return WinDefDlgProc(hwnd, msg, mp1, mp2);
}


void main(int argc, char * argv[])
{
   char szClientClass[] = "mixer.child";
   HMQ hmq;
   QMSG qmsg;
   HPOINTER hptr;
   HSWITCH hSwitch;                       /* Switch entry handle        */
   REAL_SWCNTRL SwitchData;               /* Switch control data block  */
   PID pid;

   hab = WinInitialize(0) ;
   hmq = WinCreateMsgQueue(hab, 0) ;
   WinRegisterClass(hab, szClientClass, MixerDlgProc, 0, 0);
   hwndMixer = WinLoadDlg(HWND_DESKTOP, NULLHANDLE, MixerDlgProc, NULLHANDLE, IDM_MIXERDLG, NULL);
   hptr = WinLoadPointer(HWND_DESKTOP, NULLHANDLE, ID_MIXER);
   WinSendMsg(hwndMixer, WM_SETICON, (MPARAM) hptr, NULL);

   WinQueryWindowProcess(hwndMixer, &pid, NULL);
   SwitchData.hwnd = hwndMixer;
   SwitchData.hwndIcon = NULL;
   SwitchData.hprog = NULL;
   SwitchData.idProcess = pid;
   SwitchData.idSession = 0;
   SwitchData.uchVisibility = SWL_VISIBLE;
   SwitchData.fbJump = SWL_JUMPABLE;
   SwitchData.bProgType = PROG_PM;
   strcpy(SwitchData.szSwtitle,title);
   hSwitch = WinCreateSwitchEntry(hab, (PSWCNTRL)&SwitchData);





   while (WinGetMsg (hab, &qmsg, NULLHANDLE, 0, 0))
       WinDispatchMsg (hab, &qmsg);

   WinRemoveSwitchEntry(hSwitch);
   WinDestroyWindow(hwndMixer);
   WinDestroyMsgQueue(hmq);
   WinTerminate(hab);
}


/* end of file pmmixer.c */


