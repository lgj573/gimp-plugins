/* cel.c -- KISS CEL file format plug-in for The GIMP
 * (copyright) 1997,1998 Nick Lamb (njl195@zepler.org.uk)
 *
 * Skeleton cloned from Michael Sweet's PNG plug-in. KISS format courtesy
 * of the KISS/GS documentation. Problem reports to the above address
 */

/* History:
 * 0.1  Very limited functionality (modern 4bit only)
 * 0.2  Default palette (nice yellows) is automatically used
 * 0.3  Support for the older (pre KISS/GS) cell format
 * 0.4  First support for saving images
 * 0.5  Show copyright date, not version number, thanks to DbBrowser
 * 0.6  File dialogs, palette handling, better magic behaviour
 * 0.7  Handle interactivity settings, tidy up
 * 1.0  Fixed for GIMP 0.99.27 running on GTK+ 1.0.0, and released
 * 1.1  Oops, #include unistd.h, thanks Tamito Kajiyama
 * 1.2  Changed email address, tidied up
 * 1.3  Added g_message features, fixed Saving bugs...
 * 1.4  Offsets work (needed them for a nice example set)
 *
 * Possible future additions:
 *  +   Save (perhaps optionally?) the palette in a KCF
 */
#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "libgimp/stdplugins-intl.h"

static void query (void);
static void run   (const gchar      *name,
                   gint              nparams,
                   const GimpParam  *param,
                   gint             *nreturn_vals,
                   GimpParam       **return_vals);

static gint      load_palette   (FILE        *fp,
                                 guchar       palette[]);
static gint32    load_image     (const gchar *file,
                                 const gchar *brief);
static gint      save_image     (const gchar *file,
                                 const gchar *brief,
                                 gint32       image,
                                 gint32       layer);
static void      palette_dialog (const gchar *title);
static gboolean  need_palette   (const gchar *file);


/* Globals... */

GimpPlugInInfo  PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};

static gchar *palette_file = NULL;
static gsize  data_length  = 0;

/* Let GIMP library handle initialisation (and inquisitive users) */

MAIN ()

/* GIMP queries plug-in for parameters etc. */

static void
query (void)
{
  static GimpParamDef load_args[] =
  {
    { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
    { GIMP_PDB_STRING, "filename", "Filename to load image from" },
    { GIMP_PDB_STRING, "raw_filename", "Name entered" },
    { GIMP_PDB_STRING, "palette_filename", "Filename to load palette from" }
  };
  static GimpParamDef load_return_vals[] =
  {
    { GIMP_PDB_IMAGE, "image", "Output image" }
  };

  static GimpParamDef save_args[] =
  {
    { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE, "image", "Input image" },
    { GIMP_PDB_DRAWABLE, "drawable", "Drawable to save" },
    { GIMP_PDB_STRING, "filename", "Filename to save image to" },
    { GIMP_PDB_STRING, "raw_filename", "Name entered" },
    { GIMP_PDB_STRING, "palette_filename", "Filename to save palette to" },
  };

  gimp_install_procedure ("file_cel_load",
                          "Loads files in KISS CEL file format",
                          "This plug-in loads individual KISS cell files.",
                          "Nick Lamb",
                          "Nick Lamb <njl195@zepler.org.uk>",
                          "May 1998",
                          "<Load>/CEL",
                          NULL,
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (load_args),
                          G_N_ELEMENTS (load_return_vals),
                          load_args, load_return_vals);

  gimp_install_procedure ("file_cel_save",
                          "Saves files in KISS CEL file format",
                          "This plug-in saves individual KISS cell files.",
                          "Nick Lamb",
                          "Nick Lamb <njl195@zepler.org.uk>",
                          "May 1998",
                          "<Save>/CEL",
                          "INDEXEDA",
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (save_args), 0,
                          save_args, NULL);

  gimp_register_magic_load_handler ("file_cel_load",
                                    "cel",
                                    "",
                                    "0,string,KiSS\\040");
  gimp_register_save_handler       ("file_cel_save",
                                    "cel",
                                    "");
}

static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam   values[2]; /* Return values */
  GimpRunMode        run_mode;
  GimpPDBStatusType  status = GIMP_PDB_SUCCESS;
  gint32             image;

  run_mode = param[0].data.d_int32;

  INIT_I18N ();

  /* Set up default return values */

  *nreturn_vals = 1;
  *return_vals  = values;
  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;

  if (strcmp (name, "file_cel_load") == 0)
    {
      if (run_mode != GIMP_RUN_NONINTERACTIVE)
        {
          gimp_get_data ("file_cel_save:length", &data_length);
          if (data_length > 0)
            {
              palette_file = g_malloc (data_length);
              gimp_get_data ("file_cel_save:data", palette_file);
            }
          else
            {
              palette_file = g_strdup ("*.kcf");
              data_length = strlen (palette_file) + 1;
            }
        }

      if (run_mode == GIMP_RUN_NONINTERACTIVE)
        {
          palette_file = param[3].data.d_string;
          data_length = strlen (palette_file) + 1;
        }
      else if (run_mode == GIMP_RUN_INTERACTIVE)
        {
          /* Let user choose KCF palette (cancel ignores) */
          if (need_palette (param[1].data.d_string))
            palette_dialog (_("Load KISS Palette"));

          gimp_set_data ("file_cel_save:length", &data_length, sizeof (gsize));
          gimp_set_data ("file_cel_save:data", palette_file, data_length);
        }

      image = load_image (param[1].data.d_string, param[2].data.d_string);

      if (image != -1)
        {
          *nreturn_vals = 2;
          values[1].type         = GIMP_PDB_IMAGE;
          values[1].data.d_image = image;
        }
      else
        {
          status = GIMP_PDB_EXECUTION_ERROR;
        }
    }
  else if (strcmp (name, "file_cel_save") == 0)
    {
      if (! save_image (param[3].data.d_string, param[4].data.d_string,
                        param[1].data.d_int32, param[2].data.d_int32))
        {
          status = GIMP_PDB_EXECUTION_ERROR;
        }
      else
        {
          gimp_set_data ("file_cel_save:length", &data_length, sizeof (gsize));
          gimp_set_data ("file_cel_save:data", palette_file, data_length);
        }
    }
  else
    {
      status = GIMP_PDB_CALLING_ERROR;
    }

  values[0].data.d_status = status;
}

/* Peek into the file to determine whether we need a palette */
static gboolean
need_palette (const gchar *file)
{
  FILE *fp = fopen (file, "rb");
  guchar header[32];

  fread (header, 32, 1, fp);
  fclose (fp);

  return header[5] < 32;
}

/* Load CEL image into The GIMP */

static gint32
load_image (const gchar *file,
            const gchar *brief)
{
  FILE      *fp;            /* Read file pointer */
  gchar     *progress;      /* Title for progress display */
  guchar     header[32];    /* File header */
  gint       height, width, /* Dimensions of image */
             offx, offy,    /* Layer offets */
             colours,       /* Number of colours */
             bpp;           /* Bits per pixel */

  gint32     image,         /* Image */
             layer;         /* Layer */
  guchar    *palette,       /* 24 bit palette */
            *buffer,        /* Temporary buffer */
            *line;          /* Pixel data */
  GimpDrawable *drawable;   /* Drawable for layer */
  GimpPixelRgn  pixel_rgn;  /* Pixel region for layer */

  gint       i, j, k;       /* Counters */


  /* Open the file for reading */
  fp = fopen (file, "r");

  if (fp == NULL)
    {
      g_message (_("Could not open '%s' for reading: %s"),
                 file, g_strerror (errno));
      return -1;
    }

  progress = g_strdup_printf (_("Opening '%s'..."), brief);
  gimp_progress_init (progress);
  g_free (progress);

  /* Get the image dimensions and create the image... */

  fread (header, 4, 1, fp);

  if (strncmp (header, "KiSS", 4))
    {
      colours= 16;
      bpp = 4;
      width= header[0] + (256 * header[1]);
      height= header[2] + (256 * header[3]);
      offx= 0;
      offy= 0;
    }
  else
    { /* New-style image file, read full header */
      fread (header, 28, 1, fp);
      bpp = header[1];
      if (bpp == 24)
        colours = -1;
      else
        colours = (1 << header[1]);
      width = header[4] + (256 * header[5]);
      height = header[6] + (256 * header[7]);
      offx = header[8] + (256 * header[9]);
      offy = header[10] + (256 * header[11]);
    }

  if (bpp == 32)
    image = gimp_image_new (width + offx, height + offy, GIMP_RGB);
  else
    image = gimp_image_new (width + offx, height + offy, GIMP_INDEXED);

  if (image == -1)
    {
      g_message (_("Can't create a new image"));
      return -1;
    }

  gimp_image_set_filename (image, file);

  /* Create an indexed-alpha layer to hold the image... */
  if (bpp == 32)
    layer = gimp_layer_new (image, _("Background"), width, height,
                            GIMP_RGBA_IMAGE, 100, GIMP_NORMAL_MODE);
  else
    layer = gimp_layer_new (image, _("Background"), width, height,
                            GIMP_INDEXEDA_IMAGE, 100, GIMP_NORMAL_MODE);
  gimp_image_add_layer (image, layer, 0);
  gimp_layer_set_offsets (layer, offx, offy);

  /* Get the drawable and set the pixel region for our load... */

  drawable = gimp_drawable_get (layer);

  gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, 0, drawable->width,
                       drawable->height, TRUE, FALSE);

  /* Read the image in and give it to the GIMP a line at a time */
  buffer = g_new (guchar, width * 4);
  line   = g_new (guchar, (width + 1) * 4);

  for (i = 0; i < height && !feof(fp); ++i)
    {
      switch (bpp)
        {
        case 4:
          fread (buffer, (width+1)/2, 1, fp);
          for (j = 0, k = 0; j < width*2; j+= 4, ++k)
            {
              if (buffer[k] / 16 == 0)
                {
                  line[j]= 16;
                  line[j+1]= 0;
                }
              else
                {
                  line[j]= (buffer[k] / 16) - 1;
                  line[j+1]= 255;
                }
              if (buffer[k] % 16 == 0)
                {
                  line[j+2]= 16;
                  line[j+3]= 0;
                }
              else
                {
                  line[j+2]= (buffer[k] % 16) - 1;
                  line[j+3]= 255;
                }
            }
          break;

        case 8:
          fread (buffer, width, 1, fp);
          for (j = 0, k = 0; j < width*2; j+= 2, ++k)
            {
              if (buffer[k] == 0)
                {
                  line[j]= 255;
                  line[j+1]= 0;
                }
              else
                {
                  line[j]= buffer[k] - 1;
                  line[j+1]= 255;
                }
            }
          break;

        case 32:
          fread (line, width*4, 1, fp);
          /* The CEL file order is BGR so we need to swap B and R
           * to get the Gimp RGB order.
           */
          for (j= 0; j < width; j++)
            {
              guint8 tmp = line[j*4];
              line[j*4] = line[j*4+2];
              line[j*4+2] = tmp;
            }
          break;

        default:
          g_message (_("Unsupported bit depth (%d)!"), bpp);
          return -1;
        }

      gimp_pixel_rgn_set_rect (&pixel_rgn, line, 0, i, drawable->width, 1);
      gimp_progress_update ((float) i / (float) height);
    }

  /* Close image files, give back allocated memory */

  fclose (fp);
  g_free (buffer);
  g_free (line);

  if (bpp != 32)
    {
      /* Use palette from file or otherwise default grey palette */
      palette = g_new (guchar, colours*3);

      /* Open the file for reading if user picked one */
      if (palette_file == NULL)
        {
          fp = NULL;
        }
      else
        {
          fp = fopen (palette_file, "r");
        }

      if (fp != NULL)
        {
          colours = load_palette (fp, palette);
          fclose (fp);
        }
      else
        {
          for (i= 0; i < colours; ++i)
            {
              palette[i*3] = palette[i*3+1] = palette[i*3+2]= i * 256 / colours;
            }
        }

      gimp_image_set_cmap (image, palette + 3, colours - 1);

      /* Close palette file, give back allocated memory */

      g_free (palette);
    }

  /* Now get everything redrawn and hand back the finished image */

  gimp_drawable_flush (drawable);
  gimp_drawable_detach (drawable);

  return (image);
}

static gint
load_palette (FILE   *fp,
              guchar  palette[])
{
  guchar        header[32];     /* File header */
  guchar        buffer[2];
  int           i, bpp, colours= 0;

  fread (header, 4, 1, fp);
  if (!strncmp(header, "KiSS", 4))
    {
      fread (header+4, 28, 1, fp);
      bpp = header[5];
      colours = header[8] + header[9] * 256;
      if (bpp == 12)
        {
          for (i= 0; i < colours; ++i)
            {
              fread (buffer, 1, 2, fp);
              palette[i*3]= buffer[0] & 0xf0;
              palette[i*3+1]= (buffer[1] & 0x0f) * 16;
              palette[i*3+2]= (buffer[0] & 0x0f) * 16;
            }
        }
      else
        {
          fread (palette, colours, 3, fp);
        }
    }
  else
    {
      colours = 16; bpp = 12;
      fseek (fp, 0, SEEK_SET);
      for (i= 0; i < colours; ++i)
        {
          fread (buffer, 1, 2, fp);
          palette[i*3] = buffer[0] & 0xf0;
          palette[i*3+1] = (buffer[1] & 0x0f) * 16;
          palette[i*3+2] = (buffer[0] & 0x0f) * 16;
        }
    }

  return colours;
}

static gint
save_image (const gchar *file,
            const gchar *brief,
            gint32       image,
            gint32       layer)
{
  FILE          *fp;            /* Write file pointer */
  char          *progress;      /* Title for progress display */
  guchar         header[32];    /* File header */
  gint           bpp;           /* Bit per pixel */
  gint           colours, type; /* Number of colours, type of layer */
  gint           offx, offy;    /* Layer offsets */

  guchar        *buffer;        /* Temporary buffer */
  guchar        *line;          /* Pixel data */
  GimpDrawable  *drawable;      /* Drawable for layer */
  GimpPixelRgn   pixel_rgn;     /* Pixel region for layer */

  gint           i, j, k;       /* Counters */

  /* Check that this is an indexed image, fail otherwise */
  type = gimp_drawable_type (layer);
  if (type != GIMP_INDEXEDA_IMAGE)
    bpp = 32;
  else
    bpp = 4;

  /* Find out how offset this layer was */
  gimp_drawable_offsets (layer, &offx, &offy);

  drawable = gimp_drawable_get (layer);

  /* Open the file for writing */
  fp = fopen (file, "w");

  if (fp == NULL)
    {
      g_message (_("Could not open '%s' for writing: %s"),
                 file, g_strerror (errno));
      return FALSE;
    }

  progress = g_strdup_printf (_("Saving '%s'..."), brief);
  gimp_progress_init (progress);
  g_free (progress);

  /* Headers */
  memset (header, 0, 32);
  strcpy (header, "KiSS");
  header[4]= 0x20;

  /* Work out whether to save as 8bit or 4bit */
  if (bpp < 32)
    {
      g_free (gimp_image_get_cmap (image, &colours));
      if (colours > 15)
        {
          header[5] = 8;
        }
      else
        {
          header[5] = 4;
        }
    }
  else
    header[5] = 32;

  /* Fill in the blanks ... */
  header[8]  = drawable->width % 256;
  header[9]  = drawable->width / 256;
  header[10] = drawable->height % 256;
  header[11] = drawable->height / 256;
  header[12] = offx % 256;
  header[13] = offx / 256;
  header[14] = offy % 256;
  header[15] = offy / 256;
  fwrite (header, 32, 1, fp);

  /* Arrange for memory etc. */
  gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, 0, drawable->width,
                       drawable->height, TRUE, FALSE);
  buffer = g_new (guchar, drawable->width*4);
  line = g_new (guchar, (drawable->width+1) * 4);

  /* Get the image from the GIMP one line at a time and write it out */
  for (i = 0; i < drawable->height; ++i)
    {
      gimp_pixel_rgn_get_rect (&pixel_rgn, line, 0, i, drawable->width, 1);
      memset (buffer, 0, drawable->width);

      if (bpp == 32)
        {
          for (j = 0; j < drawable->width; j++)
            {
              buffer[4*j] = line[4*j+2];     /* B */
              buffer[4*j+1] = line[4*j+1];   /* G */
              buffer[4*j+2] = line[4*j+0];   /* R */
              buffer[4*j+3] = line[4*j+3];   /* Alpha */
            }
          fwrite (buffer, drawable->width, 4, fp);
        }
      else if (colours > 16)
        {
          for (j = 0, k = 0; j < drawable->width*2; j+= 2, ++k)
            {
              if (line[j+1] > 127)
                {
                  buffer[k]= line[j] + 1;
                }
            }
          fwrite (buffer, drawable->width, 1, fp);
        }
      else
        {
          for (j = 0, k = 0; j < drawable->width*2; j+= 4, ++k)
            {
              buffer[k] = 0;
              if (line[j+1] > 127)
                {
                  buffer[k] += (line[j] + 1)<< 4;
                }
              if (line[j+3] > 127)
                {
                  buffer[k] += (line[j+2] + 1);
                }
            }
          fwrite (buffer, (drawable->width+1)/2, 1, fp);
        }

      gimp_progress_update ((float) i / (float) drawable->height);
    }

  /* Close files, give back allocated memory */
  fclose (fp);
  g_free (buffer);
  g_free (line);

  return TRUE;
}

static void
palette_dialog (const gchar *title)
{
  GtkWidget *dialog;

  gimp_ui_init ("CEL", FALSE);

  dialog = gtk_file_selection_new (title);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  gtk_file_selection_set_filename (GTK_FILE_SELECTION (dialog), palette_file);

  gimp_help_connect (dialog, gimp_standard_help_func,
                     "filters/cel.html", NULL);

  gtk_widget_show (dialog);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
      g_free (palette_file);
      palette_file =
        g_strdup (gtk_file_selection_get_filename (GTK_FILE_SELECTION (dialog)));
      data_length = strlen (palette_file) + 1;
    }

  gtk_widget_destroy (dialog);
}
