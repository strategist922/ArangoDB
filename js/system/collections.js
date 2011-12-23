////////////////////////////////////////////////////////////////////////////////
/// @brief system actions for collections
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                            administration actions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ActionsAdmin
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about all collections
///
/// @REST{GET /_system/collections}
///
/// Returns information about all collections of the database. The returned
/// array contains the following entries.
///
/// - path: The server directory containing the database.
/// - collections : An associative array of all collections.
///
/// An entry of collections is again an associative array containing the
/// following entries.
///
/// - name: The name of the collection.
/// - status: The status of the collection. 1 = new born, 2 = unloaded,
///     3 = loaded, 4 = corrupted.
///
/// @verbinclude rest15
////////////////////////////////////////////////////////////////////////////////

defineSystemAction("collections",
  function (req, res) {
    var colls;
    var coll;
    var result;

    colls = db._collections();
    result = {
      path : db._path,
      collections : {}
    };

    for (var i = 0;  i < colls.length;  ++i) {
      coll = colls[i];

      result.collections[coll._name] = {
        id : coll._id,
        name : coll._name,
        status : coll.status(),
        figures : coll.figures()
      };
    }

    actionResult(req, res, 200, result);
  }
);

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a collection
///
/// @REST{GET /_system/collection/load?collection=@FA{identifier}}
///
/// Loads a collection into memory.
///
/// @verbinclude restX
////////////////////////////////////////////////////////////////////////////////

defineSystemAction("collection/load",
  function (req, res) {
    try {
      req.collection.load();

      actionResult(req, res, 204);
    }
    catch (err) {
      actionError(req, res, err);
    }
  },
  {
    parameters : {
      collection : "collection-identifier"
    }
  }
);

////////////////////////////////////////////////////////////////////////////////
/// @brief information about a collection
///
/// @REST{GET /_system/collection/info?collection=@FA{identifier}}
///
/// Returns information about a collection
///
/// @verbinclude rest16
////////////////////////////////////////////////////////////////////////////////

defineSystemAction("collection/info",
  function (req, res) {
    try {
      result = {};
      result.id = req.collection._id;
      result.name = req.collection._name;
      result.status = req.collection.status();
      result.figures = req.collection.figures();

      actionResult(req, res, 200, result);
    }
    catch (err) {
      actionError(req, res, err);
    }
  },
  {
    parameters : {
      collection : "collection-identifier"
    }
  }
);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
