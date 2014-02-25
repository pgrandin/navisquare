#include "oauth.h"
#include <glib.h>
#include <navit/main.h>
#include <navit/debug.h>
#include <navit/point.h>
#include <navit/navit.h>
#include <navit/callback.h>
#include <navit/event.h>
#include <navit/config_.h>
#include <curl/curl.h>
#include <jansson.h>

char *coords = "near=Berkeley,%20CA&radius=20000";
char *categoryId = "4bf58dd8d48988d1df941735";
char *api_version = "20131016";

extern const char *client_secret;
extern const char *client_id;

#define URL_FORMAT   "https://api.foursquare.com/v2/venues/search?%s&categoryId=%s&client_id=%s&client_secret=%s&v=%s"
#define URL_SIZE     256

struct string
{
  char *ptr;
  size_t len;
};

void
init_string (struct string *s)
{
  s->len = 0;
  s->ptr = malloc (s->len + 1);
  if (s->ptr == NULL)
    {
      fprintf (stderr, "malloc() failed\n");
      exit (EXIT_FAILURE);
    }
  s->ptr[0] = '\0';
}

size_t
writefunc (void *ptr, size_t size, size_t nmemb, struct string *s)
{
  size_t new_len = s->len + size * nmemb;
  s->ptr = realloc (s->ptr, new_len + 1);
  if (s->ptr == NULL)
    {
      fprintf (stderr, "realloc() failed\n");
      exit (EXIT_FAILURE);
    }
  memcpy (s->ptr + s->len, ptr, size * nmemb);
  s->ptr[new_len] = '\0';
  s->len = new_len;

  return size * nmemb;
}


struct foursquare
{
  struct navit *navit;
  struct callback *callback;
  struct event_idle *idle;
};

static void
foursquare_perform_query ()
{
  CURL *curl;
  CURLcode res;
  char *text;
  char url[URL_SIZE];

  json_t *root;
  json_error_t error;

  snprintf (url, URL_SIZE, URL_FORMAT, coords, categoryId, client_id,
	    client_secret, api_version);

  curl = curl_easy_init ();
  if (curl)
    {
      struct string s;
      init_string (&s);

      curl_easy_setopt (curl, CURLOPT_URL, url);
      curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, writefunc);
      curl_easy_setopt (curl, CURLOPT_WRITEDATA, &s);
      res = curl_easy_perform (curl);
      curl_easy_cleanup (curl);
      if (!s.ptr)
	{
	  printf ("Something is wrong\n");
	}


      root = json_loads (s.ptr, 0, &error);
      free (s.ptr);

      if (!root)
	{
	  fprintf (stderr, "error: on line %d: %s\n", error.line, error.text);
	  return 1;
	}

      json_t *response, *venues;
      response = json_object_get (root, "response");
      if (!json_is_object (response))
	{
	  fprintf (stderr, "error: response is not an object\n");
	  json_decref (root);
	  return 1;
	}
      venues = json_object_get (response, "venues");
      if (!json_is_array (venues))
	{
	  fprintf (stderr, "error: venues is not an array\n");
	  json_decref (root);
	  return 1;
	}
      int i;
      for (i = 0; i < json_array_size (venues); i++)
	{
	  printf (" * %i ", i);
	  json_t *venue, *name, *location;

	  venue = json_array_get (venues, i);
	  if (!json_is_object (venue))
	    {
	      fprintf (stderr, "error: commit data %d is not an object\n",
		       i + 1);
	      json_decref (root);
	      return 1;
	    }
	  else
	    {
	      name = json_object_get (venue, "name");
	      if (!json_is_string (name))
		{
		  fprintf (stderr,
			   "error: venue.name %d: sha is not a string\n",
			   i + 1);
		  json_decref (root);
		  return 1;
		}
	      location = json_object_get (venue, "location");
	      if (!json_is_object (location))
		{
		  fprintf (stderr,
			   "error: venue.location %d: sha is not an object\n",
			   i + 1);
		  json_decref (root);
		  return 1;
		}
	      printf ("Found %s at %i:)\n", json_string_value (name),
		      json_integer_value (json_object_get
					  (location, "distance")));
	    }
	}
      json_decref (root);
    }
}

static void
foursquare_foursquare_idle (struct foursquare *foursquare)
{
  //
}

static void
foursquare_navit_init (struct navit *nav)
{
  dbg (0, "foursquare_navit_init\n");
  struct foursquare *foursquare = g_new0 (struct foursquare, 1);
  foursquare->navit = nav;
  foursquare->callback =
    callback_new_1 (callback_cast (foursquare_foursquare_idle), foursquare);
  event_add_idle (500, foursquare->callback);
  dbg (0, "Callback created successfully\n");
}

static void
foursquare_navit (struct navit *nav, int add)
{
  struct attr callback;
  if (add)
    {
      dbg (0, "adding callback\n");
      callback.type = attr_callback;
      callback.u.callback =
	callback_new_attr_0 (callback_cast (foursquare_navit_init),
			     attr_navit);
      navit_add_attr (nav, &callback);
    }
}


void
plugin_init (void)
{
  dbg (0, "foursquare init\n");
  struct attr callback, navit;
  struct attr_iter *iter;
  callback.type = attr_callback;
  callback.u.callback =
    callback_new_attr_0 (callback_cast (foursquare_navit), attr_navit);
  config_add_attr (config, &callback);
  iter = config_attr_iter_new ();
  while (config_get_attr (config, attr_navit, &navit, iter))
    foursquare_navit_init (navit.u.navit);
  config_attr_iter_destroy (iter);
}
