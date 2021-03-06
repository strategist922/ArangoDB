////////////////////////////////////////////////////////////////////////////////
/// @brief geo index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "geo-index.h"

#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"
#include "VocBase/document-collection.h"
#include "VocBase/voc-shaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a double value from an array
////////////////////////////////////////////////////////////////////////////////

static bool ExtractDoubleArray (TRI_shaper_t* shaper,
                                TRI_shaped_json_t const* document,
                                TRI_shape_pid_t pid,
                                double* result,
                                bool* missing) {
  TRI_shape_t const* shape;
  TRI_shaped_json_t json;
  bool ok;

  *missing = false;

  ok = TRI_ExtractShapedJsonVocShaper(shaper, document, 0, pid, &json, &shape);

  if (! ok) {
    return false;
  }

  if (shape == NULL) {
    *missing = true;
    return false;
  }
  else if (json._sid == shaper->_sidNumber) {
    *result = * (double*) json._data.data;
    return true;
  }
  else if (json._sid == shaper->_sidNull) {
    *missing = true;
    return false;
  }
  else {
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a double value from a list
////////////////////////////////////////////////////////////////////////////////

static bool ExtractDoubleList (TRI_shaper_t* shaper,
                               TRI_shaped_json_t const* document,
                               TRI_shape_pid_t pid,
                               double* latitude,
                               double* longitude,
                               bool* missing) {
  TRI_shape_t const* shape;
  TRI_shaped_json_t entry;
  TRI_shaped_json_t list;
  bool ok;
  size_t len;

  *missing = false;

  ok = TRI_ExtractShapedJsonVocShaper(shaper, document, 0, pid, &list, &shape);

  if (! ok) {
    return false;
  }

  if (shape == NULL) {
    *missing = true;
    return false;
  }

  // in-homogenous list
  if (shape->_type == TRI_SHAPE_LIST) {
    len = TRI_LengthListShapedJson((const TRI_list_shape_t*) shape, &list);

    if (len < 2) {
      return false;
    }

    // latitude
    ok = TRI_AtListShapedJson((const TRI_list_shape_t*) shape, &list, 0, &entry);

    if (! ok || entry._sid != shaper->_sidNumber) {
      return false;
    }

    *latitude = * (double*) entry._data.data;

    // longitude
    ok = TRI_AtListShapedJson((const TRI_list_shape_t*) shape, &list, 1, &entry);

    if (! ok || entry._sid != shaper->_sidNumber) {
      return false;
    }

    *longitude = * (double*) entry._data.data;

    return true;
  }

  // homogenous list
  else if (shape->_type == TRI_SHAPE_HOMOGENEOUS_LIST) {
    const TRI_homogeneous_list_shape_t* hom;

    hom = (const TRI_homogeneous_list_shape_t*) shape;

    if (hom->_sidEntry != shaper->_sidNumber) {
      return false;
    }

    len = TRI_LengthHomogeneousListShapedJson((const TRI_homogeneous_list_shape_t*) shape, &list);

    if (len < 2) {
      return false;
    }

    // latitude
    ok = TRI_AtHomogeneousListShapedJson((const TRI_homogeneous_list_shape_t*) shape, &list, 0, &entry);

    if (! ok) {
      return false;
    }

    *latitude = * (double*) entry._data.data;

    // longitude
    ok = TRI_AtHomogeneousListShapedJson((const TRI_homogeneous_list_shape_t*) shape, &list, 1, &entry);

    if (! ok) {
      return false;
    }

    *longitude = * (double*) entry._data.data;

    return true;
  }

  // homogeneous list
  else if (shape->_type == TRI_SHAPE_HOMOGENEOUS_SIZED_LIST) {
    const TRI_homogeneous_sized_list_shape_t* hom;

    hom = (const TRI_homogeneous_sized_list_shape_t*) shape;

    if (hom->_sidEntry != shaper->_sidNumber) {
      return false;
    }

    len = TRI_LengthHomogeneousSizedListShapedJson((const TRI_homogeneous_sized_list_shape_t*) shape, &list);

    if (len < 2) {
      return false;
    }

    // latitude
    ok = TRI_AtHomogeneousSizedListShapedJson((const TRI_homogeneous_sized_list_shape_t*) shape, &list, 0, &entry);

    if (! ok) {
      return false;
    }

    *latitude = * (double*) entry._data.data;

    // longitude
    ok = TRI_AtHomogeneousSizedListShapedJson((const TRI_homogeneous_sized_list_shape_t*) shape, &list, 1, &entry);

    if (! ok) {
      return false;
    }

    *longitude = * (double*) entry._data.data;

    return true;
  }

  // null
  else if (shape->_type == TRI_SHAPE_NULL) {
    *missing = true;
  }

  // ups
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                         GEO INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief return the index type name
////////////////////////////////////////////////////////////////////////////////

static const char* TypeNameGeo1Index (TRI_index_t const* idx) {
  return "geo1";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON description of a geo index, location is a list
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonGeo1Index (TRI_index_t* idx) {
  TRI_json_t* json;
  TRI_json_t* fields;
  TRI_primary_collection_t* primary;
  TRI_shape_path_t const* path;
  char const* location;
  TRI_geo_index_t* geo;

  geo = (TRI_geo_index_t*) idx;
  primary = idx->_collection;

  // convert location to string
  path = primary->_shaper->lookupAttributePathByPid(primary->_shaper, geo->_location);

  if (path == 0) {
    return NULL;
  }

  location = TRI_NAME_SHAPE_PATH(path);

  // create json
  json = TRI_JsonIndex(TRI_CORE_MEM_ZONE, idx);

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "geoJson", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, geo->_geoJson));
  
  // "constraint" and "unique" are identical for geo indexes. 
  // we return it for downwards-compatibility
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "constraint", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, idx->_unique));

  if (idx->_unique) {
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "ignoreNull", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, idx->_ignoreNull));
  }

  fields = TRI_CreateListJson(TRI_CORE_MEM_ZONE);
  TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, location));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "fields", fields);

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the index type name
////////////////////////////////////////////////////////////////////////////////

static const char* TypeNameGeo2Index (TRI_index_t const* idx) {
  return "geo2";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON description of a geo index, two attributes
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonGeo2Index (TRI_index_t* idx) {
  TRI_json_t* json;
  TRI_json_t* fields;
  TRI_primary_collection_t* primary;
  TRI_shape_path_t const* path;
  char const* latitude;
  char const* longitude;
  TRI_geo_index_t* geo;

  geo = (TRI_geo_index_t*) idx;
  primary = idx->_collection;

  // convert latitude to string
  path = primary->_shaper->lookupAttributePathByPid(primary->_shaper, geo->_latitude);

  if (path == 0) {
    return NULL;
  }

  latitude = TRI_NAME_SHAPE_PATH(path);

  // convert longitude to string
  path = primary->_shaper->lookupAttributePathByPid(primary->_shaper, geo->_longitude);

  if (path == 0) {
    return NULL;
  }

  longitude = TRI_NAME_SHAPE_PATH(path);

  // create json
  json = TRI_JsonIndex(TRI_CORE_MEM_ZONE, idx);

  // "constraint" and "unique" are identical for geo indexes. 
  // we return it for downwards-compatibility
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "constraint", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, idx->_unique));

  if (idx->_unique) {
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "ignoreNull", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, geo->base._ignoreNull));
  }

  fields = TRI_CreateListJson(TRI_CORE_MEM_ZONE);
  TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, latitude));
  TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, longitude));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "fields", fields);

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a new document
////////////////////////////////////////////////////////////////////////////////

static int InsertGeoIndex (TRI_index_t* idx, 
                           TRI_doc_mptr_t const* doc,
                           const bool isRollback) {
  GeoCoordinate gc;
  TRI_shaped_json_t shapedJson;
  TRI_geo_index_t* geo;
  TRI_shaper_t* shaper;
  bool missing;
  bool ok;
  double latitude;
  double longitude;
  int res;

  geo = (TRI_geo_index_t*) idx;
  shaper = geo->base._collection->_shaper;

  // lookup latitude and longitude
  TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, doc->_data);

  if (geo->_location != 0) {
    if (geo->_geoJson) {
      ok = ExtractDoubleList(shaper, &shapedJson, geo->_location, &longitude, &latitude, &missing);
    }
    else {
      ok = ExtractDoubleList(shaper, &shapedJson, geo->_location, &latitude, &longitude, &missing);
    }
  }
  else {
    ok = ExtractDoubleArray(shaper, &shapedJson, geo->_latitude, &latitude, &missing);
    ok = ok && ExtractDoubleArray(shaper, &shapedJson, geo->_longitude, &longitude, &missing);
  }

  if (! ok) {
    if (idx->_unique) {
      if (idx->_ignoreNull && missing) {
        return TRI_ERROR_NO_ERROR;
      }
      else {
        return TRI_set_errno(TRI_ERROR_ARANGO_GEO_INDEX_VIOLATED);
      }
    }
    else {
      return TRI_ERROR_NO_ERROR;
    }
  }

  // and insert into index
  gc.latitude = latitude;
  gc.longitude = longitude;

  gc.data = CONST_CAST(doc);

  res = GeoIndex_insert(geo->_geoIndex, &gc);

  if (res == -1) {
    LOG_WARNING("found duplicate entry in geo-index, should not happen");
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }
  else if (res == -2) {
    return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  }
  else if (res == -3) {
    if (idx->_unique) {
      LOG_DEBUG("illegal geo-coordinates, ignoring entry");
      return TRI_set_errno(TRI_ERROR_ARANGO_GEO_INDEX_VIOLATED);
    }
    else {
      return TRI_ERROR_NO_ERROR;
    }
  }
  else if (res < 0) {
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief erases a document
////////////////////////////////////////////////////////////////////////////////

static int RemoveGeoIndex (TRI_index_t* idx, 
                           TRI_doc_mptr_t const* doc,
                           const bool isRollback) {
  GeoCoordinate gc;
  TRI_shaped_json_t shapedJson;
  TRI_geo_index_t* geo;
  TRI_shaper_t* shaper;
  bool missing;
  bool ok;
  double latitude;
  double longitude;

  geo = (TRI_geo_index_t*) idx;
  shaper = geo->base._collection->_shaper;
  TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, doc->_data);

  // lookup OLD latitude and longitude
  if (geo->_location != 0) {
    ok = ExtractDoubleList(shaper, &shapedJson, geo->_location, &latitude, &longitude, &missing);
  }
  else {
    ok = ExtractDoubleArray(shaper, &shapedJson, geo->_latitude, &latitude, &missing);
    ok = ok && ExtractDoubleArray(shaper, &shapedJson, geo->_longitude, &longitude, &missing);
  }

  // and remove old entry
  if (ok) {
    gc.latitude = latitude;
    gc.longitude = longitude;

    gc.data = CONST_CAST(doc);

    // ignore non-existing elements in geo-index
    GeoIndex_remove(geo->_geoIndex, &gc);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a geo-index for lists
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateGeo1Index (struct TRI_primary_collection_s* primary,
                                  char const* locationName,
                                  TRI_shape_pid_t location,
                                  bool geoJson,
                                  bool unique,
                                  bool ignoreNull) {
  TRI_geo_index_t* geo;
  TRI_index_t* idx;
  char* ln;

  geo = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_geo_index_t), false);
  idx = &geo->base;

  TRI_InitVectorString(&idx->_fields, TRI_CORE_MEM_ZONE);

  idx->typeName = TypeNameGeo1Index;
  TRI_InitIndex(idx, TRI_IDX_TYPE_GEO1_INDEX, primary, unique, true);

  idx->_ignoreNull = ignoreNull;

  idx->json     = JsonGeo1Index;
  idx->insert   = InsertGeoIndex;
  idx->remove   = RemoveGeoIndex;

  ln = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, locationName);
  TRI_PushBackVectorString(&idx->_fields, ln);

  geo->_geoIndex = GeoIndex_new();

  // oops, out of memory?
  if (geo->_geoIndex == NULL) {
    TRI_DestroyVectorString(&idx->_fields);
    TRI_Free(TRI_CORE_MEM_ZONE, geo);
    return NULL;
  }

  geo->_variant    = geoJson ? INDEX_GEO_COMBINED_LAT_LON : INDEX_GEO_COMBINED_LON_LAT;
  geo->_location   = location;
  geo->_latitude   = 0;
  geo->_longitude  = 0;
  geo->_geoJson    = geoJson;

  GeoIndex_assignMethod(&(idx->indexQuery), TRI_INDEX_METHOD_ASSIGNMENT_QUERY);
  GeoIndex_assignMethod(&(idx->indexQueryFree), TRI_INDEX_METHOD_ASSIGNMENT_FREE);
  GeoIndex_assignMethod(&(idx->indexQueryResult), TRI_INDEX_METHOD_ASSIGNMENT_RESULT);

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a geo-index for arrays
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateGeo2Index (struct TRI_primary_collection_s* primary,
                                  char const* latitudeName,
                                  TRI_shape_pid_t latitude,
                                  char const* longitudeName,
                                  TRI_shape_pid_t longitude,
                                  bool unique,
                                  bool ignoreNull) {
  TRI_geo_index_t* geo;
  TRI_index_t* idx;
  char* lat;
  char* lon;

  geo = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_geo_index_t), false);
  idx = &geo->base;

  TRI_InitVectorString(&idx->_fields, TRI_CORE_MEM_ZONE);

  idx->typeName = TypeNameGeo2Index;
  TRI_InitIndex(idx, TRI_IDX_TYPE_GEO2_INDEX, primary, unique, true);

  idx->_ignoreNull = ignoreNull;
  
  idx->json     = JsonGeo2Index;
  idx->insert   = InsertGeoIndex;
  idx->remove   = RemoveGeoIndex;

  lat = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, latitudeName);
  lon = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, longitudeName);
  TRI_PushBackVectorString(&idx->_fields, lat);
  TRI_PushBackVectorString(&idx->_fields, lon);

  geo->_geoIndex = GeoIndex_new();

  // oops, out of memory?
  if (geo->_geoIndex == NULL) {
    TRI_DestroyVectorString(&idx->_fields);
    TRI_Free(TRI_CORE_MEM_ZONE, geo);
    return NULL;
  }

  geo->_variant    = INDEX_GEO_INDIVIDUAL_LAT_LON;
  geo->_location   = 0;
  geo->_latitude   = latitude;
  geo->_longitude  = longitude;

  GeoIndex_assignMethod(&(idx->indexQuery), TRI_INDEX_METHOD_ASSIGNMENT_QUERY);
  GeoIndex_assignMethod(&(idx->indexQueryFree), TRI_INDEX_METHOD_ASSIGNMENT_FREE);
  GeoIndex_assignMethod(&(idx->indexQueryResult), TRI_INDEX_METHOD_ASSIGNMENT_RESULT);

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyGeoIndex (TRI_index_t* idx) {
  TRI_geo_index_t* geo;

  TRI_DestroyVectorString(&idx->_fields);

  geo = (TRI_geo_index_t*) idx;

  GeoIndex_free(geo->_geoIndex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeGeoIndex (TRI_index_t* idx) {
  TRI_DestroyGeoIndex(idx);
  TRI_Free(TRI_CORE_MEM_ZONE, idx);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up all points within a given radius
////////////////////////////////////////////////////////////////////////////////

GeoCoordinates* TRI_WithinGeoIndex (TRI_index_t* idx,
                                    double lat,
                                    double lon,
                                    double radius) {
  TRI_geo_index_t* geo;
  GeoCoordinate gc;

  geo = (TRI_geo_index_t*) idx;
  gc.latitude = lat;
  gc.longitude = lon;

  return GeoIndex_PointsWithinRadius(geo->_geoIndex, &gc, radius);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up the nearest points
////////////////////////////////////////////////////////////////////////////////

GeoCoordinates* TRI_NearestGeoIndex (TRI_index_t* idx,
                                     double lat,
                                     double lon,
                                     size_t count) {
  TRI_geo_index_t* geo;
  GeoCoordinate gc;

  geo = (TRI_geo_index_t*) idx;
  gc.latitude = lat;
  gc.longitude = lon;

  return GeoIndex_NearestCountPoints(geo->_geoIndex, &gc, (int) count);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
