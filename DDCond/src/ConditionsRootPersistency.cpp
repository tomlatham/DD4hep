//==========================================================================
//  AIDA Detector description implementation 
//--------------------------------------------------------------------------
// Copyright (C) Organisation europeenne pour la Recherche nucleaire (CERN)
// All rights reserved.
//
// For the licensing terms see $DD4hepINSTALL/LICENSE.
// For the list of contributors see $DD4hepINSTALL/doc/CREDITS.
//
// Author     : M.Frank
//
//==========================================================================

// Framework include files
#include "DD4hep/Printout.h"
#include "DDCond/ConditionsSlice.h"
#include "DDCond/ConditionsIOVPool.h"
#include "DDCond/ConditionsRootPersistency.h"

#include "TFile.h"
#include "TTimeStamp.h"

typedef dd4hep::cond::ConditionsRootPersistency __ConditionsRootPersistency;
ClassImp(__ConditionsRootPersistency)


using namespace std;
using namespace dd4hep;
using namespace dd4hep::cond;

namespace  {
  /// Helper to select conditions
  /*
   *  \author  M.Frank
   *  \version 1.0
   *  \ingroup DD4HEP_CONDITIONS
   */
  struct Scanner : public Condition::Processor   {
    ConditionsRootPersistency::pool_type& pool;
    /// Constructor
    Scanner(ConditionsRootPersistency::pool_type& p) : pool(p) {}
    /// Conditions callback for object processing
    virtual int process(Condition c)  const override  {
      pool.push_back(c.ptr());
      return 1;
    }
  };
  struct DurationStamp  {
    TTimeStamp start;
    ConditionsRootPersistency* object = 0;
    DurationStamp(ConditionsRootPersistency* obj) : object(obj)  {
    }
    ~DurationStamp()  {
      TTimeStamp stop;
      object->duration = stop.AsDouble()-start.AsDouble();
    }
  };
}

/// Default constructor
ConditionsRootPersistency::ConditionsRootPersistency() : TNamed()   {
}

/// Initializing constructor
ConditionsRootPersistency::ConditionsRootPersistency(const string& name, const string& title)
  : TNamed(name.c_str(), title.c_str())
{
}

/// Default destructor
ConditionsRootPersistency::~ConditionsRootPersistency()    {
  clear();
}

/// Add conditions content to the saved. Note, that dependent conditions shall not be saved!
size_t ConditionsRootPersistency::add(const string& identifier, ConditionsPool& pool)    {
  DurationStamp stamp(this);
  conditionPools.push_back(pair<iov_key_type, pool_type>());
  pool_type&    ent = conditionPools.back().second;
  iov_key_type& key = conditionPools.back().first;
  const IOV*    iov = pool.iov;
  key.first         = identifier;
  key.second.first  = make_pair(iov->iovType->name,iov->type);
  key.second.second = iov->key();
  pool.select_all(ent);
  for(auto c : ent) c.ptr()->addRef();
  return ent.size();
}

/// Add conditions content to the saved. Note, that dependent conditions shall not be saved!
size_t ConditionsRootPersistency::add(const string& identifier, const ConditionsIOVPool& pool)    {
  size_t count = 0;
  DurationStamp stamp(this);
  for( const auto& p : pool.elements )  {
    iovPools.push_back(pair<iov_key_type, pool_type>());
    pool_type&    ent = iovPools.back().second;
    iov_key_type& key = iovPools.back().first;
    const IOV*    iov = p.second->iov;
    key.first         = identifier;
    key.second.first  = make_pair(iov->iovType->name,iov->type);
    key.second.second = p.first;
    p.second->select_all(ent);
    for(auto c : ent) c.ptr()->addRef();
    count += ent.size();
  }
  return count;
}

/// Add conditions content to the saved. Note, that dependent conditions shall not be saved!
size_t ConditionsRootPersistency::add(const string& identifier, const UserPool& pool)    {
  DurationStamp stamp(this);
  userPools.push_back(pair<iov_key_type, pool_type>());
  pool_type&    ent = userPools.back().second;
  iov_key_type& key = userPools.back().first;
  const IOV&    iov = pool.validity();
  key.first         = identifier;
  key.second.first  = make_pair(iov.iovType->name,iov.type);
  key.second.second = iov.key();
  pool.scan(Scanner(ent));
  for(auto c : ent) c.ptr()->addRef();
  return ent.size();
}

/// Open ROOT file in read mode
TFile* ConditionsRootPersistency::openFile(const string& fname)     {
  TDirectory::TContext context;
  TFile* file = TFile::Open(fname.c_str());
  if ( file && !file->IsZombie()) return file;
  except("ConditionsRootPersistency","+++ FAILED to open ROOT file %s in read-mode.",fname.c_str());
  return 0;
}

/// Clear object content and release allocated memory
void ConditionsRootPersistency::_clear(persistent_type& pool)  {
  /// Cleanup all the stuff not useful....
  for (auto& p : pool )  {
    for(Condition c : p.second )
      c.ptr()->release();
    p.second.clear();
  }
  pool.clear();
}

/// Clear object content and release allocated memory
void ConditionsRootPersistency::clear()  {
  /// Cleanup all the stuff not useful....
  _clear(conditionPools);
  _clear(userPools);
  _clear(iovPools);
}

/// Add conditions content to the saved. Note, that dependent conditions shall not be saved!
std::unique_ptr<ConditionsRootPersistency>
ConditionsRootPersistency::load(TFile* file,const string& obj)   {
  std::unique_ptr<ConditionsRootPersistency> p;
  if ( file && !file->IsZombie())    {
    TTimeStamp start;
    p.reset((ConditionsRootPersistency*)file->Get(obj.c_str()));
    TTimeStamp stop;
    if ( p.get() )    {
      p->duration = stop.AsDouble()-start.AsDouble();
      return p;
    }
    except("ConditionsRootPersistency",
           "+++ FAILED to load object %s from file %s",
           obj.c_str(), file->GetName());
  }
  except("ConditionsRootPersistency",
         "+++ FAILED to load object %s from file [Invalid file]",obj.c_str());
  return p;
}

/// Load ConditionsPool(s) and populate conditions manager
size_t ConditionsRootPersistency::_import(persistent_type&   persistent_pools,
                                          const std::string& id,
                                          const std::string& iov_type,
                                          ConditionsManager mgr)   {
  size_t count = 0;
  std::pair<bool,const IOVType*> iovTyp(false,0);
  for (auto& iovp : persistent_pools )   {
    const iov_key_type& key = iovp.first;
    if ( !(id.empty() || id=="*" || key.first == id) )
      continue;
    if ( !(iov_type.empty() || iov_type == "*" || key.second.first.first == iov_type) )
      continue;
    iovTyp = mgr.registerIOVType(key.second.first.second,key.second.first.first);
    if ( iovTyp.second )   {
      ConditionsPool* pool = mgr.registerIOV(*iovTyp.second, key.second.second);
      for (Condition c : iovp.second)   {
        Condition::Object* o = c.ptr();
        o->iov = pool->iov;
        if ( pool->insert(o->addRef()) )    {
          //printout(WARNING,"ConditionsRootPersistency","%p: [%016llX] %s -> %s",
          //         o, o->hash,o->GetName(),o->data.str().c_str());
          ++count;
        }
        else   {
          printout(WARNING,"ConditionsRootPersistency",
                   "+++ Ignore condition %s from %s iov:%s [Already present]",
                   c.name(),id.c_str(), iov_type.c_str());
        }
      }
    }
  }
  return count;
}

/// Load ConditionsIOVPool and populate conditions manager
size_t ConditionsRootPersistency::importIOVPool(const std::string& identifier,
                                                const std::string& iov_type,
                                                ConditionsManager  mgr)
{
  DurationStamp stamp(this);
  return _import(iovPools,identifier,iov_type,mgr);
}

/// Load ConditionsIOVPool and populate conditions manager
size_t ConditionsRootPersistency::importUserPool(const std::string& identifier,
                                                 const std::string& iov_type,
                                                 ConditionsManager  mgr)
{
  DurationStamp stamp(this);
  return _import(userPools,identifier,iov_type,mgr);
}

/// Load ConditionsIOVPool and populate conditions manager
size_t ConditionsRootPersistency::importConditionsPool(const std::string& identifier,
                                                       const std::string& iov_type,
                                                       ConditionsManager  mgr)
{
  DurationStamp stamp(this);
  return _import(conditionPools,identifier,iov_type,mgr);
}

/// Save the data content to a root file
int ConditionsRootPersistency::save(TFile* file)    {
  DurationStamp stamp(this);
  //TDirectory::TContext context;
  int nBytes = file->WriteTObject(this,GetName());
  return nBytes;
}

/// Save the data content to a root file
int ConditionsRootPersistency::save(const string& fname)    {
  DurationStamp stamp(this);
  //TDirectory::TContext context;
  TFile* file = TFile::Open(fname.c_str(),"RECREATE");
  if ( file && !file->IsZombie())   {
    int nBytes = save(file);
    file->Close();
    delete file;
    return nBytes;
  }
  return -1;
}