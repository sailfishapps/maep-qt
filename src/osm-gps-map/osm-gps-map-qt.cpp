#include "osm-gps-map-qt.h"
#include "osm-gps-map.h"
#undef WITH_GTK
#include "misc.h"

#include <QWidget>
#include <QPainter>

#define GCONF_KEY_ZOOM       "zoom"
#define GCONF_KEY_SOURCE     "source"
#define GCONF_KEY_LATITUDE   "latitude"
#define GCONF_KEY_LONGITUDE  "longitude"
#define GCONF_KEY_DOUBLEPIX  "double-pixel"
#define GCONF_KEY_WIKIPEDIA  "wikipedia"

#define MAP_SOURCE  OSM_GPS_MAP_SOURCE_OPENCYCLEMAP

static void osm_gps_map_qt_repaint(Maep::GpsMap *widget, OsmGpsMap *map);
static void osm_gps_map_qt_wiki(Maep::GpsMap *widget, MaepGeonamesEntry *entry, MaepWikiContext *wiki);
static void osm_gps_map_qt_places(Maep::GpsMap *widget, GSList *places, MaepSearchContext *wiki);

Maep::GpsMap::GpsMap(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
  char *path;

  const char *p = getenv("HOME");
  gint source = gconf_get_int(GCONF_KEY_SOURCE, MAP_SOURCE);
  gint zoom = gconf_get_int(GCONF_KEY_ZOOM, 3);
  gfloat lat = gconf_get_float(GCONF_KEY_LATITUDE, 50.0);
  gfloat lon = gconf_get_float(GCONF_KEY_LONGITUDE, 21.0);
  gboolean dpix = gconf_get_bool(GCONF_KEY_DOUBLEPIX, FALSE);
  bool wikipedia = gconf_get_bool(GCONF_KEY_WIKIPEDIA, FALSE);

  if(!p) p = "/tmp"; 
  path = g_strdup_printf("%s/.osm-gps-map", p);

  map = OSM_GPS_MAP(g_object_new(OSM_TYPE_GPS_MAP,
                                 "map-source",               source,
                                 "tile-cache",               OSM_GPS_MAP_CACHE_FRIENDLY,
                                 "tile-cache-base",          path,
                                 "auto-center",              FALSE,
                                 "record-trip-history",      FALSE, 
                                 "show-trip-history",        FALSE, 
                                 "gps-track-point-radius",   10,
                                 // proxy?"proxy-uri":NULL,     proxy,
                                 "double-pixel",             dpix,
                                 NULL));
  osm_gps_map_set_mapcenter(map, lat, lon, zoom);

  g_signal_connect_swapped(G_OBJECT(map), "dirty",
                           G_CALLBACK(osm_gps_map_qt_repaint), this);

  g_free(path);

  osd = osm_gps_map_osd_classic_init(map);
  wiki = maep_wiki_context_new();
  wiki_enabled = wikipedia;
  if (wiki_enabled)
    maep_wiki_context_enable(wiki, map);
  g_signal_connect_swapped(G_OBJECT(wiki), "entry-selected",
                           G_CALLBACK(osm_gps_map_qt_wiki), this);
  search = maep_search_context_new();
  g_signal_connect_swapped(G_OBJECT(search), "places-available",
                           G_CALLBACK(osm_gps_map_qt_places), this);

  drag_start_mouse_x = 0;
  drag_start_mouse_y = 0;
  drag_mouse_dx = 0;
  drag_mouse_dy = 0;

  surf = NULL;
  cr = NULL;
  img = NULL;

  forceActiveFocus();
  setAcceptedMouseButtons(Qt::LeftButton);
}
Maep::GpsMap::~GpsMap()
{
  gint zoom, source;
  gfloat lat, lon;
  gboolean dpix;

  /* get state information from map ... */
  g_object_get(map, 
	       "zoom", &zoom, 
	       "map-source", &source, 
	       "latitude", &lat, "longitude", &lon,
	       "double-pixel", &dpix,
	       NULL);
  osm_gps_map_osd_classic_free(osd);
  maep_wiki_context_enable(wiki, NULL);
  g_object_unref(wiki);
  g_object_unref(search);

  if (surf)
    cairo_surface_destroy(surf);
  if (cr)
    cairo_destroy(cr);
  if (img)
    delete(img);

  /* ... and store it in gconf */
  gconf_set_int(GCONF_KEY_ZOOM, zoom);
  gconf_set_int(GCONF_KEY_SOURCE, source);
  gconf_set_float(GCONF_KEY_LATITUDE, lat);
  gconf_set_float(GCONF_KEY_LONGITUDE, lon);
  gconf_set_bool(GCONF_KEY_DOUBLEPIX, dpix);

  gconf_set_bool(GCONF_KEY_WIKIPEDIA, wiki_enabled);

  g_object_unref(map);
}

static void osm_gps_map_qt_repaint(Maep::GpsMap *widget, OsmGpsMap *map)
{
  widget->update();
}

void Maep::GpsMap::paint(QPainter *painter)
{
  cairo_surface_t *map_surf;

  g_message("repainting %fx%f!", width(), height());

  if (!surf)
    {
      surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width(), height());
      cr = cairo_create(surf);
      img = new QImage(cairo_image_surface_get_data(surf),
                       cairo_image_surface_get_width(surf),
                       cairo_image_surface_get_height(surf),
                       QImage::Format_ARGB32);
    }

  cairo_set_source_rgb(cr, 1., 1., 1.);
  cairo_paint(cr);

  osm_gps_map_set_viewport(map, width(), height());
  map_surf = osm_gps_map_get_surface(map);
  cairo_set_source_surface(cr, map_surf,
                           drag_mouse_dx - EXTRA_BORDER,
                           drag_mouse_dy - EXTRA_BORDER);
  cairo_paint(cr);
  cairo_surface_destroy(map_surf);

#ifdef ENABLE_OSD
  osd->draw (osd, cr);
#endif

  QRectF target(0, 0,
                cairo_image_surface_get_width(surf),
                cairo_image_surface_get_height(surf));
  QRectF source(0, 0, cairo_image_surface_get_width(surf),
                cairo_image_surface_get_height(surf));
  painter->drawImage(target, *img, source);
}

#define OSM_GPS_MAP_SCROLL_STEP     (10)

void Maep::GpsMap::keyPressEvent(QKeyEvent * event)
{
  int step;

  step = width() / OSM_GPS_MAP_SCROLL_STEP;

  if (event->key() == Qt::Key_Up)
    osm_gps_map_scroll(map, 0, -step);
  else if (event->key() == Qt::Key_Down)
    osm_gps_map_scroll(map, 0, step);
  else if (event->key() == Qt::Key_Right)
    osm_gps_map_scroll(map, step, 0);
  else if (event->key() == Qt::Key_Left)
    osm_gps_map_scroll(map, -step, 0);
  else if (event->key() == Qt::Key_Plus)
    osm_gps_map_zoom_in(map);
  else if (event->key() == Qt::Key_Minus)
    osm_gps_map_zoom_out(map);
  else if (event->key() == Qt::Key_S)
    emit searchRequest();
}

void Maep::GpsMap::mousePressEvent(QMouseEvent *event)
{
#ifdef ENABLE_OSD
  /* pressed inside OSD control? */
  int step;
  osd_button_t but = 
    osd->check(osd, TRUE, event->x(), event->y());
  
  dragging = FALSE;

  step = width() / OSM_GPS_MAP_SCROLL_STEP;

  if(but != OSD_NONE)
    switch(but)
      {
      case OSD_UP:
        osm_gps_map_scroll(map, 0, -step);
        g_object_set(G_OBJECT(map), "auto-center", FALSE, NULL);
        return;

      case OSD_DOWN:
        osm_gps_map_scroll(map, 0, +step);
        g_object_set(G_OBJECT(map), "auto-center", FALSE, NULL);
        return;

      case OSD_LEFT:
        osm_gps_map_scroll(map, -step, 0);
        g_object_set(G_OBJECT(map), "auto-center", FALSE, NULL);
        return;
                
      case OSD_RIGHT:
        osm_gps_map_scroll(map, +step, 0);
        g_object_set(G_OBJECT(map), "auto-center", FALSE, NULL);
        return;
                
      case OSD_IN:
        osm_gps_map_zoom_in(map);
        return;
                
      case OSD_OUT:
        osm_gps_map_zoom_out(map);
        return;
                
      default:
        /* all custom buttons are forwarded to the application */
        if(osd->cb)
          osd->cb(but, osd->data);
        return;
      }
#endif
  if (osm_gps_map_layer_button(OSM_GPS_MAP_LAYER(wiki),
                               event->x(), event->y(), TRUE))
    return;

  dragging = TRUE;
  drag_start_mouse_x = event->x();
  drag_start_mouse_y = event->y();
}
void Maep::GpsMap::mouseReleaseEvent(QMouseEvent *event)
{
  if (dragging)
    {
      dragging = FALSE;
      osm_gps_map_scroll(map, -drag_mouse_dx, -drag_mouse_dy);
      drag_mouse_dx = 0;
      drag_mouse_dy = 0;
    }
  osm_gps_map_layer_button(OSM_GPS_MAP_LAYER(wiki),
                           event->x(), event->y(), FALSE);
}
void Maep::GpsMap::mouseMoveEvent(QMouseEvent *event)
{
  if (dragging)
    {
      drag_mouse_dx = event->x() - drag_start_mouse_x;
      drag_mouse_dy = event->y() - drag_start_mouse_y;
      update();
    }
}

void Maep::GpsMap::setWikiStatus(bool status)
{
  if (status == wiki_enabled)
    return;

  wiki_enabled = status;
  emit wikiStatusChanged(wiki_enabled);

  maep_wiki_context_enable(wiki, (wiki_enabled)?map:NULL);
}

void Maep::GpsMap::setWikiInfo(const char *title, const char *summary,
                               const char *thumbnail, const char *url,
                               float lat, float lon)
{
  wiki_title = QString(title);
  wiki_summary = QString(summary);
  wiki_thumbnail = QString(thumbnail);
  wiki_url = QString(url);
  wiki_coord = QGeoCoordinate(rad2deg(lat), rad2deg(lon));
  emit wikiURLSelected();
}

QString Maep::GpsMap::getWikiPosition()
{
  return wiki_coord.toString();
}

static void osm_gps_map_qt_wiki(Maep::GpsMap *widget, MaepGeonamesEntry *entry, MaepWikiContext *wiki)
{
  widget->setWikiInfo(entry->title, entry->summary, entry->thumbnail_url, entry->url,
                      entry->pos.rlat, entry->pos.rlon);
}

void Maep::GpsMap::setSearchResults(GSList *places)
{
  g_message("hello got %d places", g_slist_length(places));

  qDeleteAll(searchRes);
  searchRes.clear();

  for (; places; places = places->next)
    {
      Maep::GeonamesPlace *place = new Maep::GeonamesPlace((const MaepGeonamesPlace*)places->data);
      searchRes.append(place);
    }
  
  emit searchResults();
}

static void osm_gps_map_qt_places(Maep::GpsMap *widget, GSList *places, MaepSearchContext *wiki)
{
  g_message("Got %d matching places.", g_slist_length(places));
  widget->setSearchResults(places);
}

void Maep::GpsMap::setSearchRequest(const QString &request)
{
  maep_search_context_request(search, request.toLocal8Bit().data());
}

void Maep::GpsMap::setLookAt(float lat, float lon)
{
  g_message("move to %fx%f", lat, lon);
  osm_gps_map_set_center(map, rad2deg(lat), rad2deg(lon));
}
