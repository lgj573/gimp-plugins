/* Cubism --- image filter plug-in for The Gimp image manipulation program
 * Copyright (C) 1996 Spencer Kimball, Tracy Scott
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * You can contact me at quartic@polloux.fciencias.unam.mx
 * You can contact the original The Gimp authors at gimp@xcf.berkeley.edu
 * Speedups by Elliot Lee
 */
#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "libgimp/stdplugins-intl.h"


#define SCALE_WIDTH    125
#define BLACK            0
#define BG               1
#define SUPERSAMPLE      4
#define MAX_POINTS       4
#define MIN_ANGLE   -36000
#define MAX_ANGLE    36000
#define RANDOMNESS       5

typedef struct
{
  gint        npts;
  GimpVector2 pts[MAX_POINTS];
} Polygon;

typedef struct
{
  gdouble tile_size;
  gdouble tile_saturation;
  gint    bg_color;
} CubismVals;

/* Declare local functions.
 */
static void      query  (void);
static void      run    (const gchar      *name,
			 gint              nparams,
			 const GimpParam  *param,
			 gint             *nreturn_vals,
			 GimpParam       **return_vals);
static void      cubism (GimpDrawable     *drawable);

static void      fill_poly_color      (Polygon      *poly,
				       GimpDrawable *drawable,
				       guchar       *col);
static void      convert_segment      (gint       x1,
				       gint       y1,
				       gint       x2,
				       gint       y2,
				       gint       offset,
				       gint      *min,
				       gint      *max);
static void      randomize_indices    (gint       count,
				       gint      *indices);
static gdouble   calc_alpha_blend     (gdouble   *vec,
				       gdouble    one_over_dist,
				       gdouble    x,
				       gdouble    y);
static void      polygon_add_point    (Polygon   *poly,
				       gdouble    x,
				       gdouble    y);
static void      polygon_translate    (Polygon   *poly,
				       gdouble    tx,
				       gdouble    ty);
static void      polygon_rotate       (Polygon   *poly,
				       gdouble    theta);
static gint      polygon_extents      (Polygon   *poly,
				       gdouble   *min_x,
				       gdouble   *min_y,
				       gdouble   *max_x,
				       gdouble   *max_y);
static void      polygon_reset        (Polygon   *poly);

static gint      cubism_dialog        (void);

/*
 *  Local variables
 */

static CubismVals cvals =
{
  10.0,        /* tile_size */
  2.5,         /* tile_saturation */
  BLACK        /* bg_color */
};

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};


/*
 *  Functions
 */

MAIN ()

static void
query (void)
{
  static GimpParamDef args[] =
  {
    { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE, "image", "Input image" },
    { GIMP_PDB_DRAWABLE, "drawable", "Input drawable" },
    { GIMP_PDB_FLOAT, "tile_size", "Average diameter of each tile (in pixels)" },
    { GIMP_PDB_FLOAT, "tile_saturation", "Expand tiles by this amount" },
    { GIMP_PDB_INT32, "bg_color", "Background color: { BLACK (0), BG (1) }" }
  };

  gimp_install_procedure ("plug_in_cubism",
			  "Convert the input drawable into a collection of rotated squares",
			  "Help not yet written for this plug-in",
			  "Spencer Kimball & Tracy Scott",
			  "Spencer Kimball & Tracy Scott",
			  "1996",
			  N_("_Cubism..."),
			  "RGB*, GRAY*",
			  GIMP_PLUGIN,
			  G_N_ELEMENTS (args), 0,
			  args, NULL);

  gimp_plugin_menu_register ("plug_in_cubism",
                             N_("<Image>/Filters/Artistic"));
}

static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam   values[1];
  GimpDrawable      *active_drawable;
  GimpRunMode        run_mode;
  GimpPDBStatusType  status = GIMP_PDB_SUCCESS;

  INIT_I18N ();

  run_mode = param[0].data.d_int32;

  *nreturn_vals = 1;
  *return_vals = values;

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

  switch (run_mode)
    {
    case GIMP_RUN_INTERACTIVE:
      /*  Possibly retrieve data  */
      gimp_get_data ("plug_in_cubism", &cvals);

      /*  First acquire information with a dialog  */
      if (! cubism_dialog ())
	return;
      break;

    case GIMP_RUN_NONINTERACTIVE:
      /*  Make sure all the arguments are there!  */
      if (nparams != 6)
	status = GIMP_PDB_CALLING_ERROR;
      if (status == GIMP_PDB_SUCCESS)
	{
	  cvals.tile_size       = param[3].data.d_float;
	  cvals.tile_saturation = param[4].data.d_float;
	  cvals.bg_color        = param[5].data.d_int32;
	}
      if (status == GIMP_PDB_SUCCESS &&
	  (cvals.bg_color < BLACK || cvals.bg_color > BG))
	status = GIMP_PDB_CALLING_ERROR;
      break;

    case GIMP_RUN_WITH_LAST_VALS:
      /*  Possibly retrieve data  */
      gimp_get_data ("plug_in_cubism", &cvals);
      break;

    default:
      break;
    }

  /*  get the active drawable  */
  active_drawable = gimp_drawable_get (param[2].data.d_drawable);

  /*  Render the cubism effect  */
  if ((status == GIMP_PDB_SUCCESS) &&
      (gimp_drawable_is_rgb (active_drawable->drawable_id) ||
       gimp_drawable_is_gray (active_drawable->drawable_id)))
    {
      /*  set cache size  */
      gimp_tile_cache_ntiles (SQR (4 * cvals.tile_size * cvals.tile_saturation) / SQR (gimp_tile_width ()));

      cubism (active_drawable);

      /*  If the run mode is interactive, flush the displays  */
      if (run_mode != GIMP_RUN_NONINTERACTIVE)
	gimp_displays_flush ();

      /*  Store mvals data  */
      if (run_mode == GIMP_RUN_INTERACTIVE)
	gimp_set_data ("plug_in_cubism", &cvals, sizeof (CubismVals));
    }
  else if (status == GIMP_PDB_SUCCESS)
    {
      /* gimp_message ("cubism: cannot operate on indexed color images"); */
      status = GIMP_PDB_EXECUTION_ERROR;
    }

  values[0].data.d_status = status;

  gimp_drawable_detach (active_drawable);
}

static gint
cubism_dialog (void)
{
  GtkWidget *dlg;
  GtkWidget *toggle;
  GtkWidget *frame;
  GtkWidget *table;
  GtkObject *scale_data;
  gboolean   run;

  gimp_ui_init ("cubism", FALSE);

  dlg = gimp_dialog_new (_("Cubism"), "cubism",
                         NULL, 0,
			 gimp_standard_help_func, "plug-in-cubism",

			 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			 GTK_STOCK_OK,     GTK_RESPONSE_OK,

			 NULL);

  /*  parameter settings  */
  frame = gtk_frame_new (_("Parameter Settings"));
  gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  table = gtk_table_new (3, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_row_spacing (GTK_TABLE (table), 0, 4);
  gtk_container_set_border_width (GTK_CONTAINER (table), 6);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  toggle = gtk_check_button_new_with_mnemonic (_("_Use Background Color"));
  gtk_table_attach (GTK_TABLE (table), toggle, 0, 3, 0, 1,
		    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (toggle);

  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &cvals.bg_color);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
				(cvals.bg_color == BG));

  scale_data = gimp_scale_entry_new (GTK_TABLE (table), 0, 1,
				     _("_Tile Size:"), SCALE_WIDTH, 5,
				     cvals.tile_size, 0.0, 100.0, 1.0, 10.0, 1,
				     TRUE, 0, 0,
				     NULL, NULL);
  g_signal_connect (scale_data, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &cvals.tile_size);

  scale_data =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 2,
			  _("T_ile Saturation:"), SCALE_WIDTH, 5,
			  cvals.tile_saturation, 0.0, 10.0, 0.1, 1, 1,
			  TRUE, 0, 0,
			  NULL, NULL);
  g_signal_connect (scale_data, "value_changed",
                    G_CALLBACK (gimp_double_adjustment_update),
                    &cvals.tile_saturation);

  gtk_widget_show (dlg);

  run = (gimp_dialog_run (GIMP_DIALOG (dlg)) == GTK_RESPONSE_OK);

  gtk_widget_destroy (dlg);

  return run;
}

static void
cubism (GimpDrawable *drawable)
{
  GimpPixelRgn src_rgn;
  guchar       bg_col[4];
  gdouble      x, y;
  gdouble      width, height;
  gdouble      theta;
  gint         ix, iy;
  gint         rows, cols;
  gint         i, j, count;
  gint         num_tiles;
  gint         x1, y1, x2, y2;
  Polygon      poly;
  guchar       col[4];
  guchar      *dest;
  gint         bytes;
  gboolean     has_alpha;
  gint        *random_indices;
  gpointer     pr;
  GRand       *gr;

  gr = g_rand_new ();
  has_alpha = gimp_drawable_has_alpha (drawable->drawable_id);
  bytes = drawable->bpp;
  gimp_drawable_mask_bounds (drawable->drawable_id, &x1, &y1, &x2, &y2);

  /*  determine the background color  */
  if (cvals.bg_color == BLACK)
    {
      bg_col[0] = bg_col[1] = bg_col[2] = bg_col[3] = 0;
    }
  else
    {
      GimpRGB color;

      gimp_palette_get_background (&color);
      gimp_rgb_set_alpha (&color, 0.0);
      gimp_drawable_get_color_uchar (drawable->drawable_id, &color, bg_col);
    }

  gimp_progress_init (_("Cubistic Transformation"));

  cols = ((x2 - x1) + cvals.tile_size - 1) / cvals.tile_size;
  rows = ((y2 - y1) + cvals.tile_size - 1) / cvals.tile_size;

  /*  Fill the image with the background color  */
  gimp_pixel_rgn_init (&src_rgn, drawable,
		       x1, y1, (x2 - x1), (y2 - y1), TRUE, TRUE);

  for (pr = gimp_pixel_rgns_register (1, &src_rgn);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
    {
      count = src_rgn.w * src_rgn.h;
      dest = src_rgn.data;

      while (count--)
	for (i = 0; i < bytes; i++)
	  *dest++ = bg_col[i];
    }

  num_tiles = (rows + 1) * (cols + 1);
  random_indices = g_new (gint, num_tiles);
  for (i = 0; i < num_tiles; i++)
    random_indices[i] = i;

  randomize_indices (num_tiles, random_indices);

  count = 0;
  gimp_pixel_rgn_init (&src_rgn, drawable,
		       x1, y1, x2 - x1, y2 - y1, FALSE, FALSE);

  while (count < num_tiles)
    {
      i = random_indices[count] / (cols + 1);
      j = random_indices[count] % (cols + 1);
      x = j * cvals.tile_size + (cvals.tile_size / 4.0)
	- g_rand_double_range (gr, 0, cvals.tile_size/2.0) + x1;
      y = i * cvals.tile_size + (cvals.tile_size / 4.0)
	- g_rand_double_range (gr, 0, cvals.tile_size/2.0) + y1;
      width = (cvals.tile_size +
	       g_rand_double_range (gr, 0, cvals.tile_size / 4.0) -
	       cvals.tile_size / 8.0) * cvals.tile_saturation;
      height = (cvals.tile_size +
		g_rand_double_range (gr, 0, cvals.tile_size / 4.0) -
		cvals.tile_size / 8.0) * cvals.tile_saturation;
      theta = g_rand_double_range (gr, 0, 2 * G_PI);
      polygon_reset (&poly);
      polygon_add_point (&poly, -width / 2.0, -height / 2.0);
      polygon_add_point (&poly, width / 2.0, -height / 2.0);
      polygon_add_point (&poly, width / 2.0, height / 2.0);
      polygon_add_point (&poly, -width / 2.0, height / 2.0);
      polygon_rotate (&poly, theta);
      polygon_translate (&poly, x, y);

      /*  bounds check on x, y  */
      ix = CLAMP (x, x1, x2 - 1);
      iy = CLAMP (y, y1, y2 - 1);

      gimp_pixel_rgn_get_pixel (&src_rgn, col, ix, iy);

      if (!has_alpha || col[bytes - 1])
	fill_poly_color (&poly, drawable, col);

      count++;
      if ((count % 5) == 0)
	gimp_progress_update ((double) count / (double) num_tiles);
    }

  gimp_progress_update (1.0);
  g_free (random_indices);
  g_rand_free (gr);

  /*  merge the shadow, update the drawable  */
  gimp_drawable_flush (drawable);
  gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
  gimp_drawable_update (drawable->drawable_id, x1, y1, x2 - x1, y2 - y1);
}

static inline gdouble
calc_alpha_blend (gdouble *vec,
		  gdouble  one_over_dist,
		  gdouble  x,
		  gdouble  y)
{
  gdouble r;

  if (!one_over_dist)
    return 1.0;
  else
    {
      r = (vec[0] * x + vec[1] * y) * one_over_dist;
      if (r < 0.2)
	r = 0.2;
      else if (r > 1.0)
	r = 1.0;
    }
  return r;
}

static void
fill_poly_color (Polygon   *poly,
		 GimpDrawable *drawable,
		 guchar    *col)
{
  GimpPixelRgn src_rgn;
  gdouble dmin_x, dmin_y;
  gdouble dmax_x, dmax_y;
  gint xs, ys;
  gint xe, ye;
  gint min_x, min_y;
  gint max_x, max_y;
  gint size_x, size_y;
  gint * max_scanlines, *max_scanlines_iter;
  gint * min_scanlines, *min_scanlines_iter;
  gint val;
  gint alpha;
  gint bytes;
  guchar buf[4];
  gint i, j, x, y;
  gdouble sx, sy;
  gdouble ex, ey;
  gdouble xx, yy;
  gdouble vec[2];
  gdouble dist, one_over_dist;
  gint x1, y1, x2, y2;
  gint *vals, *vals_iter, *vals_end;

  sx = poly->pts[0].x;
  sy = poly->pts[0].y;
  ex = poly->pts[1].x;
  ey = poly->pts[1].y;

  dist = sqrt (SQR (ex - sx) + SQR (ey - sy));
  if (dist > 0.0)
    {
      one_over_dist = 1/dist;
      vec[0] = (ex - sx) * one_over_dist;
      vec[1] = (ey - sy) * one_over_dist;
    }
  else
    one_over_dist = 0.0;

  gimp_drawable_mask_bounds (drawable->drawable_id, &x1, &y1, &x2, &y2);
  bytes = drawable->bpp;

  polygon_extents (poly, &dmin_x, &dmin_y, &dmax_x, &dmax_y);
  min_x = (gint) dmin_x;
  min_y = (gint) dmin_y;
  max_x = (gint) dmax_x;
  max_y = (gint) dmax_y;

  size_y = (max_y - min_y) * SUPERSAMPLE;
  size_x = (max_x - min_x) * SUPERSAMPLE;

  min_scanlines = min_scanlines_iter = g_new (gint, size_y);
  max_scanlines = max_scanlines_iter = g_new (gint, size_y);
  for (i = 0; i < size_y; i++)
    {
      min_scanlines[i] = max_x * SUPERSAMPLE;
      max_scanlines[i] = min_x * SUPERSAMPLE;
    }

  if(poly->npts) {
    gint poly_npts = poly->npts;
    GimpVector2 *curptr;

    xs = (gint) (poly->pts[poly_npts-1].x);
    ys = (gint) (poly->pts[poly_npts-1].y);
    xe = (gint) poly->pts[0].x;
    ye = (gint) poly->pts[0].y;

    xs *= SUPERSAMPLE;
    ys *= SUPERSAMPLE;
    xe *= SUPERSAMPLE;
    ye *= SUPERSAMPLE;

    convert_segment (xs, ys, xe, ye, min_y * SUPERSAMPLE,
		     min_scanlines, max_scanlines);

    for (i = 1, curptr = &poly->pts[0]; i < poly_npts; i++)
      {
	xs = (gint) curptr->x;
	ys = (gint) curptr->y;
	curptr++;
	xe = (gint) curptr->x;
	ye = (gint) curptr->y;

	xs *= SUPERSAMPLE;
	ys *= SUPERSAMPLE;
	xe *= SUPERSAMPLE;
	ye *= SUPERSAMPLE;

	convert_segment (xs, ys, xe, ye, min_y * SUPERSAMPLE,
			 min_scanlines, max_scanlines);
      }
  }

  gimp_pixel_rgn_init (&src_rgn, drawable, 0, 0,
		       drawable->width, drawable->height, TRUE, TRUE);

  vals = g_new (gint, size_x);

  for (i = 0; i < size_y; i++, min_scanlines_iter++, max_scanlines_iter++)
    {
      if (! (i % SUPERSAMPLE))
	{
	  memset (vals, 0, sizeof (gint) * size_x);
	}

      yy = (gdouble)i / (gdouble)SUPERSAMPLE + min_y;

      for (j = *min_scanlines_iter; j < *max_scanlines_iter; j++)
	{
	  x = j - min_x * SUPERSAMPLE;
	  vals[x] += 255;
	}

      if (! ((i + 1) % SUPERSAMPLE))
	{
	  y = (i / SUPERSAMPLE) + min_y;

	  if (y >= y1 && y < y2)
	    {
	      for (j = 0; j < size_x; j += SUPERSAMPLE)
		{
		  x = (j / SUPERSAMPLE) + min_x;

		  if (x >= x1 && x < x2)
		    {
		      for (val = 0, vals_iter = &vals[j],
			     vals_end = &vals_iter[SUPERSAMPLE];
			   vals_iter < vals_end;
			   vals_iter++)
			val += *vals_iter;

		      val /= SQR(SUPERSAMPLE);

		      if (val > 0)
			{
			  xx = (gdouble) j / (gdouble) SUPERSAMPLE + min_x;
			  alpha = (gint) (val * calc_alpha_blend (vec, one_over_dist, xx - sx, yy - sy));
			  gimp_pixel_rgn_get_pixel (&src_rgn, buf, x, y);

#ifndef USE_READABLE_BUT_SLOW_CODE
			  {
			    guchar *buf_iter = buf,
			      *col_iter = col,
			      *buf_end = buf+bytes;

			    for(; buf_iter < buf_end; buf_iter++, col_iter++)
			      *buf_iter = ((guint)(*col_iter * alpha)
					   + (((guint)*buf_iter)
					      * (256 - alpha))) >> 8;
			  }
#else /* original, pre-ECL code */
			  for (b = 0; b < bytes; b++)
			    buf[b] = ((col[b] * alpha) + (buf[b] * (255 - alpha))) / 255;

#endif
			  gimp_pixel_rgn_set_pixel (&src_rgn, buf, x, y);
			}
		    }
		}
	    }
	}
    }

  g_free (vals);
  g_free (min_scanlines);
  g_free (max_scanlines);
}

static void
convert_segment (gint  x1,
		 gint  y1,
		 gint  x2,
		 gint  y2,
		 gint  offset,
		 gint *min,
		 gint *max)
{
  gint ydiff, y, tmp;
  gdouble xinc, xstart;

  if (y1 > y2)
    {
      tmp = y2; y2 = y1; y1 = tmp;
      tmp = x2; x2 = x1; x1 = tmp;
    }
  ydiff = (y2 - y1);

  if (ydiff)
    {
      xinc = (gdouble) (x2 - x1) / (gdouble) ydiff;
      xstart = x1 + 0.5 * xinc;
      for (y = y1 ; y < y2; y++)
	{
	  if (xstart < min[y - offset])
	    min[y-offset] = xstart;
	  if (xstart > max[y - offset])
	    max[y-offset] = xstart;

	  xstart += xinc;
	}
    }
}

static void
randomize_indices (gint  count,
		   gint *indices)
{
  gint i;
  gint index1, index2;
  gint tmp;
  GRand *gr;

  gr = g_rand_new();

  for (i = 0; i < count * RANDOMNESS; i++)
    {
      index1 = g_rand_int_range (gr, 0, count);
      index2 = g_rand_int_range (gr, 0, count);
      tmp = indices[index1];
      indices[index1] = indices[index2];
      indices[index2] = tmp;
    }

  g_rand_free (gr);
}

static void
polygon_add_point (Polygon *poly,
		   gdouble  x,
		   gdouble  y)
{
  if (poly->npts < 12)
    {
      poly->pts[poly->npts].x = x;
      poly->pts[poly->npts].y = y;
      poly->npts++;
    }
  else
    g_print ("Unable to add additional point.\n");
}

static void
polygon_rotate (Polygon *poly,
		gdouble  theta)
{
  gint i;
  gdouble ct, st;
  gdouble ox, oy;

  ct = cos (theta);
  st = sin (theta);

  for (i = 0; i < poly->npts; i++)
    {
      ox = poly->pts[i].x;
      oy = poly->pts[i].y;
      poly->pts[i].x = ct * ox - st * oy;
      poly->pts[i].y = st * ox + ct * oy;
    }
}

static void
polygon_translate (Polygon *poly,
		   gdouble  tx,
		   gdouble  ty)
{
  gint i;

  for (i = 0; i < poly->npts; i++)
    {
      poly->pts[i].x += tx;
      poly->pts[i].y += ty;
    }
}

static gint
polygon_extents (Polygon *poly,
		 gdouble *x1,
		 gdouble *y1,
		 gdouble *x2,
		 gdouble *y2)
{
  gint i;

  if (!poly->npts)
    return 0;

  *x1 = *x2 = poly->pts[0].x;
  *y1 = *y2 = poly->pts[0].y;

  for (i = 1; i < poly->npts; i++)
    {
      if (poly->pts[i].x < *x1)
	*x1 = poly->pts[i].x;
      if (poly->pts[i].x > *x2)
	*x2 = poly->pts[i].x;
      if (poly->pts[i].y < *y1)
	*y1 = poly->pts[i].y;
      if (poly->pts[i].y > *y2)
	*y2 = poly->pts[i].y;
    }

  return 1;
}

static void
polygon_reset (Polygon *poly)
{
  poly->npts = 0;
}
