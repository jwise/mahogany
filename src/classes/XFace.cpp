/*-*- c++ -*-********************************************************
 * XFace.cc -  a class encapsulating XFace handling                 *
 *                                                                  *
 * (C) 1998-1999 by Karsten Ball�der (ballueder@gmx.net)            *
 *                                                                  *
 * $Id$
 *******************************************************************/

#ifdef __GNUG__
#pragma implementation "XFace.h"
#endif

#include "Mpch.h"
#include "Mcommon.h"

#include "XFace.h"
#include "strutil.h"
#include "kbList.h"
#include  <stdio.h>

#ifdef XFACE_WITH_WXIMAGE
#   include "guidef.h"
#   include "MDialogs.h"
#   include "gui/wxIconManager.h"
#   include "PathFinder.h"
#   include "MApplication.h"
#   include "Profile.h"
#endif

#include "Mdefaults.h"

#ifdef HAVE_COMPFACE_H
   extern "C"
   {
#     include  <compface.h>
   };
#else
#  ifdef  CC_MSC
#     pragma message("No compface library found, compiling empty XFace class!")
#  else
#     warning  "No compface library found, compiling empty XFace class!"
#  endif
#endif


XFace::XFace()
{
   initialised  = false;
   data = NULL;
   xface = NULL;
}

bool
XFace::CreateFromData(const char *idata)
{
#ifndef  HAVE_COMPFACE_H
   return false;
#else
   if(data)  delete [] data;
   data = strutil_strdup(idata);
   if(xface) delete [] xface;

   xface = new char[2500];
   strcpy(xface, data);
   if(compface(xface) < 0)
   {
      delete [] xface;
      delete [] data;
      xface = data = NULL;
      return false;
   }
   //convert it:
   String out = strutil_enforceCRLF(xface);
   delete [] xface;
   xface = strutil_strdup(out);
   initialised = true;
   return true;
#endif
}

// reads from the data, not from a file in memory, it's different!
bool
XFace::CreateFromXpm(const char *xpmdata)
{
#ifndef  HAVE_COMPFACE_H
   return false;
#else
   if(data)
      delete [] data;

   char
      *buf = strutil_strdup(xpmdata),
      *ptr = buf,
      *token,
      buffer[20],
      zero = 0, one = 0;
   int
      n,i,l;
   long
      value;
   String
      dataString,
      tstr;

   initialised = false;
   do
   {
      token = strsep(&ptr, "\n\r");
      if(! token)
   break;
      if(zero == 0 || one == 0)
      {
         strncpy(buffer,token+4,8);
         tstr = buffer;
         strutil_tolower(tstr);

   if(tstr == "#000000" || tstr == "gray0")
            zero = token[0];
   else if(tstr == "#ffffff"
                 || tstr >= "gray100"
                 || tstr >= "white")
      one = token[0];
      }
      else  // now the data will follow
   break;
   }
   while(token);
   if(! token) // something went wrong
   {
      delete [] buf;
      return false;
   }

   for(l = 0; l < 48; l++)
   {
      for(n = 0; n <= 32; n+= 16)
      {
   value = 0;
   for(i = 0; i < 16; i++)
   {
      if(token[n+i] == one)
         value += 1;
      if(i != 15)
               value <<= 1;
   }
         value = value ^ 0xffff;
   sprintf(buffer,"0x%04lX", value);
   dataString += buffer;
         dataString += ',';
      }
      dataString += '\n';
      token = strsep(&ptr, "\n\r");
      if(l < 47 && ! token)
      {
   delete [] buf;
   return false;
      }
   }
   delete [] buf;
   return CreateFromData(dataString);
#endif
}


#if XFACE_WITH_WXIMAGE

#include   <wx/image.h>

/* static */
wxImage
XFace::GetXFaceImg(const String &filename, bool *hasimg, class wxWindow *parent)
{
   bool success = false;
   wxImage img;
   if(filename.Length())
   {
      img = wxIconManager::LoadImage(filename, &success);
      if(! success)
      {
         String msg;
         msg.Printf(_("Could not load XFace file '%s'."),
                    filename.c_str());
      }
   }
   if(success)
   {
      if(img.GetWidth() != 48 || img.GetHeight() != 48)
         img = img.Scale(48,48);
      // Now, check if we have some non-B&W colours:
      int intensity;
      for(int y = 0; y < 48; y++)
         for(int x = 0; x < 48; x++)
         {
            intensity = img.GetRed(x,y) + img.GetGreen(x,y) +
               img.GetBlue(x,y);
            if(intensity >= (3*255)/2 && intensity != 3*255)
               img.SetRGB(x,y,255,255,255);
            if(intensity <= (3*255)/2 && intensity != 0)
               img.SetRGB(x,y,0,0,0);
         }
      if(hasimg) *hasimg = true;
   }
   else
   {
      PathFinder pf(READ_APPCONFIG(MP_ICONPATH), true);
      pf.AddPaths(mApplication->GetLocalDir()+"/icons", true);
      pf.AddPaths(mApplication->GetGlobalDir()+"/icons", true);
      String name = pf.FindFile("xface.xpm", &success);
      if(success)
         img = wxIconManager::LoadImage(name, &success);
      if(hasimg) *hasimg = success;
   }
   return img;
}


/* static */
String
XFace::ConvertImgToXFaceData(wxImage &img)
{
   int l, n, i;
   int value;
   String dataString;
   String tmp;

   for(l = 0; l < 48; l++)
   {
      for(n = 0; n <= 32; n+= 16)
      {
   value = 0;
   for(i = 0; i < 16; i++)
   {
      if(img.GetRed(n+i,l) != 0)
         value += 1;
      if(i != 15)
               value <<= 1;
   }
         value = value ^ 0xffff;
   tmp.Printf("0x%04lX", value);
   dataString += tmp;
         dataString += ',';
      }
      dataString += '\n';
   }
   return dataString;
}


bool
XFace::CreateFromFile(const char *filename)
{
   wxImage img = GetXFaceImg(filename );
   String datastring = ConvertImgToXFaceData(img);
   return CreateFromData(datastring);
}


#if 0
/**
   Create an XFace from a wxImage.
   @param image image to read
   @return true on success
*/
bool
XFace::CreateFromImage(wxImage *image)
{
#ifndef  HAVE_COMPFACE_H
   return false;
#else
   if(data)
      delete [] data;

   char
      buffer[20];
   int
      n,i,y;
   long
      value;
   String
      dataString,
      tstr;

   initialised = false;
   for(y = 0; y < 48; y++)
   {
      for(n = 0; n <= 32; n+= 16)
      {
   value = 0;
   for(i = 0; i < 16; i++)
   {
            if(image->GetRed(n+i,y) != 0)  // evaluate red only
         value += 1;
      if(i != 15)
               value <<= 1;
   }
         value = value ^ 0xffff;
   sprintf(buffer,"0x%04lX", value);
   dataString += buffer;
         dataString += ',';
      }
      dataString += '\n';
   }
   return CreateFromData(dataString);
#endif
}
#endif

#endif // with wxImage

bool
XFace::CreateFromXFace(const char *xfacedata)
{
#ifndef HAVE_COMPFACE_H
   return false;
#else
   if(data) delete [] data;
   if(xface) delete [] xface;
   initialised = false;

   xface = new char [2500];
   strncpy(xface, xfacedata, 2500);
   data = new char [5000];
   strncpy(data, xface, 5000);
   if(uncompface(data) < 0)
   {
      delete [] data;
      delete [] xface;
      data = xface = NULL;
      return false;
   }
   String out = strutil_enforceCRLF(xface);
   delete [] xface;
   xface = strutil_strdup(out);
   initialised = true;
   return true;
#endif
}

bool
XFace::CreateXpm(String &xpm)
{
#ifndef HAVE_COMPFACE_H
   return false;
#else
   int
      l,c,q;
   char
      *ptr, *buf, *token;

   buf = strutil_strdup(data);
   ptr = buf;

   xpm = "";
   xpm +=
      "/* XPM */\n"
      "static char *xface[] = {\n"
      "/* width height num_colors chars_per_pixel */\n"
      "\"    48    48        2            1\",\n"
      "/* colors */\n"
      "\"# c #000000\",\n"
      "\". c #ffffff\",\n";
   for(l = 0; l < 48; l++)
   {
      xpm += '"';
      for(c = 0; c < 3; c++)
      {
   token = strsep(&ptr,",\n\r");
   if(strlen(token) == 0)
      token = strsep(&ptr, ",\n\r");  // skip end of line
   if(token)
   {
      token += 2;  // skip  0x
      for(q = 0; q < 4; q++)
      {
         switch(token[q])
         {
         case '0':
      xpm += "...."; break;
         case '1':
      xpm += "...#"; break;
         case '2':
      xpm += "..#."; break;
         case '3':
      xpm += "..##"; break;
         case '4':
      xpm += ".#.."; break;
         case '5':
      xpm += ".#.#"; break;
         case '6':
      xpm += ".##."; break;
         case '7':
      xpm += ".###"; break;
         case '8':
      xpm += "#..."; break;
         case '9':
      xpm += "#..#"; break;
         case 'a': case 'A':
      xpm += "#.#."; break;
         case 'b': case 'B':
      xpm += "#.##"; break;
         case 'c': case 'C':
      xpm += "##.."; break;
         case 'd': case 'D':
      xpm += "##.#"; break;
         case 'e': case 'E':
      xpm += "###."; break;
         case 'f': case 'F':
      xpm += "####"; break;
         default:
      break;
         }
      }

   }
      }
      xpm += '"';
      if(l < 47)
   xpm += ",\n";
      else
   xpm += "\n};\n";
   }
   return true;
#endif
}

bool
XFace::CreateXpm(char ***xpm)
{
#ifndef HAVE_COMPFACE_H
   return false;
#else
   int
      l,c,q;
   char
      *ptr, *buf, *token;
   int
      line = 0;
   String
      tmp;

   *xpm = (char **) malloc(sizeof(char *)*52);

   buf = strutil_strdup(data);
   ptr = buf;

   (*xpm)[line++] = strutil_strdup(" 48 48 2 1");
   (*xpm)[line++] = strutil_strdup("# c #000000");
   (*xpm)[line++] = strutil_strdup(". c #ffffff");
   for(l = 0; l < 48; l++)
   {
      tmp = "";
      for(c = 0; c < 3; c++)
      {
   token = strsep(&ptr,",\n\r");
   if(strlen(token) == 0)
      token = strsep(&ptr, ",\n\r");  // skip end of line
   if(token)
   {
      token += 2;  // skip  0x
      for(q = 0; q < 4; q++)
      {
         switch(token[q])
         {
         case '0':
      tmp += "...."; break;
         case '1':
      tmp += "...#"; break;
         case '2':
      tmp += "..#."; break;
         case '3':
      tmp += "..##"; break;
         case '4':
      tmp += ".#.."; break;
         case '5':
      tmp += ".#.#"; break;
         case '6':
      tmp += ".##."; break;
         case '7':
      tmp += ".###"; break;
         case '8':
      tmp += "#..."; break;
         case '9':
      tmp += "#..#"; break;
         case 'a': case 'A':
      tmp += "#.#."; break;
         case 'b': case 'B':
      tmp += "#.##"; break;
         case 'c': case 'C':
      tmp += "##.."; break;
         case 'd': case 'D':
      tmp += "##.#"; break;
         case 'e': case 'E':
      tmp += "###."; break;
         case 'f': case 'F':
      tmp += "####"; break;
         default:
      break;
         }
      }

   }
      }
      (*xpm)[line++] = strutil_strdup(tmp);
   }
   (*xpm)[line++] = NULL;
   return true;
#endif
}

String
XFace::GetHeaderLine(void) const
{
   if(xface)
      return xface;
   else
      return "";
}

XFace::~XFace()
{
   if(data)  delete[] data;
   if(xface)  delete[] xface;
   initialised = false;
}


