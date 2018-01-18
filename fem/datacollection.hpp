// Copyright (c) 2010, Lawrence Livermore National Security, LLC. Produced at
// the Lawrence Livermore National Laboratory. LLNL-CODE-443211. All Rights
// reserved. See file COPYRIGHT for details.
//
// This file is part of the MFEM library. For more information and source code
// availability see http://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License (as published by the Free
// Software Foundation) version 2.1 dated February 1999.

#ifndef MFEM_DATACOLLECTION
#define MFEM_DATACOLLECTION

#include "../config/config.hpp"
#include "gridfunc.hpp"
#ifdef MFEM_USE_MPI
#include "pgridfunc.hpp"
#endif
#include <string>
#include <map>

#include <iostream>

namespace mfem
{

/// Lightweight adaptor over an std::map from strings to pointer to T
template<typename T>
class NamedFieldsMap
{
public:
   typedef std::map<std::string, T*> MapType;
   typedef typename MapType::iterator iterator;
   typedef typename MapType::const_iterator const_iterator;

   void Register(const std::string& fname, T* field, bool own_data)
   {
      T*& ref = field_map[fname];
      if (own_data)
      {
         delete ref; // if newly allocated -> ref is null -> OK
      }
      ref = field;

      std::cout << " Added '" << fname << "'; "
                << " field is " << (field == NULL ? "null" : "not null;")
                << " map has " << NumFields() << " entries."
                << std::endl ;
   }

   void Deregister(const std::string& fname, bool own_data)
   {
      iterator it = field_map.find(fname);
      if ( it != field_map.end() )
      {
         if (own_data)
         {
            delete it->second;
         }
         field_map.erase(it);
      }
   }

   void DeleteData(bool own_data)
   {
      for (iterator it = field_map.begin(); it != field_map.end(); ++it)
      {
         if (own_data)
         {
            delete it->second;
         }
         it->second = NULL;
      }
   }

   bool Has(const std::string& fname) const
   {
      return field_map.find(fname) != field_map.end();
   }

   T* Get(const std::string& fname) const
   {
      const_iterator it = field_map.find(fname);

      std::cout << " Searched for '" << fname << "'; "
                << " was " << (it != field_map.end() ? "there" : "not there; ")
                << " map has " << NumFields() << " entries."
                << std::endl ;

      return it != field_map.end() ? it->second : NULL;
   }

   const MapType& GetMap() const { return field_map; }

   int NumFields() const { return field_map.size(); }

   iterator begin() { return field_map.begin(); }
   const_iterator begin() const { return field_map.begin(); }

   iterator end() { return field_map.end(); }
   const_iterator end() const { return field_map.end(); }

   iterator find(const std::string& fname)
   { return field_map.find(fname); }

   const_iterator find(const std::string& fname) const
   { return field_map.find(fname); }

   void clear() { field_map.clear(); }

protected:
   MapType field_map;
};


/** A class for collecting finite element data that is part of the same
    simulation. Currently, this class groups together grid functions (fields),
    quadrature functions (q-fields), and the mesh that they are defined on. */
class DataCollection
{
private:
   typedef NamedFieldsMap<GridFunction> GFieldMap;
   typedef NamedFieldsMap<QuadratureFunction> QFieldMap;
public:
   typedef typename GFieldMap::MapType FieldMapType;
   typedef typename GFieldMap::iterator FieldMapIterator;
   typedef typename GFieldMap::const_iterator FieldMapConstIterator;

   typedef typename QFieldMap::MapType QFieldMapType;
   typedef typename QFieldMap::iterator QFieldMapIterator;
   typedef typename QFieldMap::const_iterator QFieldMapConstIterator;

protected:
   /// Name of the collection, used as a directory name when saving
   std::string name;

   /// A path where the directory with results is saved.
   /// If not empty, it has '/' at the end.
   std::string prefix_path;

   /** A FieldMap mapping registered fields' names as keys (std::string)
       and their GridFunction pointers as values. */
   GFieldMap field_map;

   /** A FieldMap mapping fields' names as keys (std::string)
       and their QuadratureFunction pointers as values. */
   QFieldMap q_field_map;

   /// The (common) mesh for the collected fields
   Mesh *mesh;

   /** Time cycle; for time-dependent simulations cycle >= 0, otherwise = -1.
       When cycle >= 0, it is appended to directory names. */
   int cycle;
   /// Physical time (for time-dependent simulations)
   double time;

   /// Time step i.e. delta_t (for time-dependent simulations)
   double time_step;

   /// Serial or parallel run?
   bool serial;
   /// Append rank to any output file names.
   bool appendRankToFileName;

   /// MPI rank (in parallel)
   int myid;
   /// Number of MPI ranks (in parallel)
   int num_procs;

   /// Precision (number of digits) used for the text output of doubles
   int precision;
   /// Number of digits used for the cycle and MPI rank in filenames
   int pad_digits_cycle, pad_digits_rank;

   /// Default value for precision
   static const int precision_default = 6;
   /// Default value for pad_digits_*
   static const int pad_digits_default = 6;

   /// Output mesh format: 0 - serial format (default), 1 - parallel format
   int format;

   /// Should the collection delete its mesh and fields
   bool own_data;

   /// Error state
   int error;

   /// Delete data owned by the DataCollection keeping field information
   void DeleteData();
   /// Delete data owned by the DataCollection including field information
   void DeleteAll();

   std::string GetFieldFileName(const std::string &field_name);

   /// Save one field to disk, assuming the collection directory exists
   void SaveOneField(const FieldMapIterator &it);

   /// Save one q-field to disk, assuming the collection directory exists
   void SaveOneQField(const QFieldMapIterator &it);

   // Helper method
   static int create_directory(const std::string &dir_name,
                               const Mesh *mesh, int myid);

public:
   /// Initialize the collection with its name and Mesh.
   /** When @a mesh_ is NULL, then the real mesh can be set with SetMesh(). */
   DataCollection(const std::string& collection_name, Mesh *mesh_ = NULL);

   /// Add a grid function to the collection
   virtual void RegisterField(const std::string& field_name, GridFunction *gf)
   { field_map.Register(field_name, gf, own_data); }

   /// Remove a grid function from the collection
   virtual void DeregisterField(const std::string& field_name)
   { field_map.Deregister(field_name, own_data); }

   /// Add a QuadratureFunction to the collection.
   virtual void RegisterQField(const std::string& q_field_name,
                               QuadratureFunction *qf)
   { q_field_map.Register(q_field_name, qf, own_data); }


   /// Remove a QuadratureFunction from the collection
   virtual void DeregisterQField(const std::string& field_name)
   { q_field_map.Deregister(field_name, own_data); }

   /// Check if a grid function is part of the collection
   bool HasField(const std::string& name) const
   { return field_map.Has(name); }

   /// Get a pointer to a grid function in the collection.
   /** Returns NULL if @a field_name is not in the collection. */
   GridFunction *GetField(const std::string& field_name)
   { return field_map.Get(field_name); }

#ifdef MFEM_USE_MPI
   /// Get a pointer to a parallel grid function in the collection.
   /** Returns NULL if @a field_name is not in the collection.
       @note The GridFunction pointer stored in the collection is statically
       cast to ParGridFunction pointer. */
   ParGridFunction *GetParField(const std::string& field_name)
   { return static_cast<ParGridFunction*>(GetField(field_name)); }
#endif

   /// Check if a QuadratureFunction with the given name is in the collection.
   bool HasQField(const std::string& q_field_name) const
   { return q_field_map.Has(q_field_name); }

   /// Get a pointer to a QuadratureFunction in the collection.
   /** Returns NULL if @a field_name is not in the collection. */
   QuadratureFunction *GetQField(const std::string& q_field_name)
   { return q_field_map.Get(q_field_name); }

   /// Get a const reference to the internal field map.
   /** The keys in the map are the field names and the values are pointers to
       GridFunction%s. */
   const FieldMapType &GetFieldMap() const
   { return field_map.GetMap(); }

   /// Get a const reference to the internal q-field map.
   /** The keys in the map are the q-field names and the values are pointers to
       QuadratureFunction%s. */
   const QFieldMapType &GetQFieldMap() const
   { return q_field_map.GetMap(); }

   /// Get a pointer to the mesh in the collection
   Mesh *GetMesh() { return mesh; }
   /// Set/change the mesh associated with the collection
   virtual void SetMesh(Mesh *new_mesh);

   /// Set time cycle (for time-dependent simulations)
   void SetCycle(int c) { cycle = c; }
   /// Set physical time (for time-dependent simulations)
   void SetTime(double t) { time = t; }

   /// Set the simulation time step (for time-dependent simulations)
   void SetTimeStep(double ts) { time_step = ts; }

   /// Get time cycle (for time-dependent simulations)
   int GetCycle() const { return cycle; }
   /// Get physical time (for time-dependent simulations)
   double GetTime() const { return time; }
   /// Get the simulation time step (for time-dependent simulations)
   double GetTimeStep() const { return time_step; }

   /// Get the name of the collection
   const std::string& GetCollectionName() const { return name; }
   /// Set the ownership of collection data
   void SetOwnData(bool o) { own_data = o; }

   /// Set the precision (number of digits) used for the text output of doubles
   void SetPrecision(int prec) { precision = prec; }
   /// Set the number of digits used for both the cycle and the MPI rank
   void SetPadDigits(int digits) { pad_digits_cycle=pad_digits_rank = digits; }
   /// Set the number of digits used for the cycle
   void SetPadDigitsCycle(int digits) { pad_digits_cycle = digits; }
   /// Set the number of digits used for the MPI rank in filenames
   void SetPadDigitsRank(int digits) { pad_digits_rank = digits; }
   /** @brief Set the desired output mesh format: 0 - serial format (default),
       1 - parallel format. */
   void SetFormat(int fmt) { format = fmt; }

   /// Set the path where the DataCollection will be saved.
   void SetPrefixPath(const std::string &prefix);

   /// Get the path where the DataCollection will be saved.
   const std::string &GetPrefixPath() const { return prefix_path; }

   /** Save the collection to disk. By default, everything is saved in a
       directory with name "collection_name" or "collection_name_cycle" for
       time-dependent simulations. */
   virtual void Save();
   /// Save the mesh, creating the collection directory.
   virtual void SaveMesh();
   /// Save one field, assuming the collection directory already exists.
   virtual void SaveField(const std::string &field_name);
   /// Save one q-field, assuming the collection directory already exists.
   virtual void SaveQField(const std::string &q_field_name);

   /// Load the collection. Not implemented in the base class DataCollection.
   virtual void Load(int cycle_ = 0);

   /// Delete the mesh and fields if owned by the collection
   virtual ~DataCollection();

   /// Errors returned by Error()
   enum { NO_ERROR = 0, READ_ERROR = 1, WRITE_ERROR = 2 };

   /// Get the current error state
   int Error() const { return error; }
   /// Reset the error state
   void ResetError(int err = NO_ERROR) { error = err; }
};


/// Helper class for VisIt visualization data
class VisItFieldInfo
{
public:
   std::string association;
   int num_components;
   VisItFieldInfo() { association = ""; num_components = 0; }
   VisItFieldInfo(std::string _association, int _num_components)
   { association = _association; num_components = _num_components; }
};

/// Data collection with VisIt I/O routines
class VisItDataCollection : public DataCollection
{
protected:
   // Additional data needed in the VisIt root file, which describes the mesh
   // and all the fields in the collection
   int spatial_dim, topo_dim;
   int visit_max_levels_of_detail;
   std::map<std::string, VisItFieldInfo> field_info_map;
   typedef std::map<std::string, VisItFieldInfo>::iterator FieldInfoMapIterator;

   /// Prepare the VisIt root file in JSON format for the current collection
   std::string GetVisItRootString();
   /// Read in a VisIt root file in JSON format
   void ParseVisItRootString(const std::string& json);

   // Helper functions for Load()
   void LoadVisItRootFile(const std::string& root_name);
   void LoadMesh();
   void LoadFields();

public:
   /// Constructor. The collection name is used when saving the data.
   /** If @a mesh_ is NULL, then the mesh can be set later by calling either
       SetMesh() or Load(). The latter works only in serial. */
   VisItDataCollection(const std::string& collection_name, Mesh *mesh_ = NULL);

   /// Set/change the mesh associated with the collection
   virtual void SetMesh(Mesh *new_mesh);

   /// Add a grid function to the collection and update the root file
   virtual void RegisterField(const std::string& field_name, GridFunction *gf);

   /// Set VisIt parameter: maximum levels of detail for the MultiresControl
   void SetMaxLevelsOfDetail(int max_levels_of_detail);

   /** Delete all data owned by VisItDataCollection including field data
       information. */
   void DeleteAll();

   /// Save the collection and a VisIt root file
   virtual void Save();

   /// Save a VisIt root file for the collection
   void SaveRootFile();

   /// Load the collection based on its VisIt data (described in its root file)
   virtual void Load(int cycle_ = 0);

   /// We will delete the mesh and fields if we own them
   virtual ~VisItDataCollection() {}
};

}

#endif
