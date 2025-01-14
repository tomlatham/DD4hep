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
#include <DD4hep/detail/DetectorInterna.h>
#include <DD4hep/detail/ConditionsInterna.h>
#include <DD4hep/detail/AlignmentsInterna.h>
#include <DD4hep/AlignmentTools.h>
#include <DD4hep/DetectorTools.h>
#include <DD4hep/Printout.h>
#include <DD4hep/World.h>
#include <DD4hep/Detector.h>

using namespace dd4hep;
    
namespace {
  static std::string s_empty_string;
}

/// Default constructor
DetElement::Processor::Processor()   {
}

/// Default destructor
DetElement::Processor::~Processor()   {
}

/// Clone constructor
DetElement::DetElement(Object* det_data, const std::string& det_name, const std::string& det_type)
  : Handle<DetElementObject>(det_data)
{
  this->assign(det_data, det_name, det_type);
}

/// Constructor for a new subdetector element
DetElement::DetElement(const std::string& det_name, const std::string& det_type, int det_id) {
  assign(new Object(det_name,det_id), det_name, det_type);
  ptr()->id = det_id;
}

/// Constructor for a new subdetector element
DetElement::DetElement(const std::string& det_name, int det_id) {
  assign(new Object(det_name,det_id), det_name, "");
  ptr()->id = det_id;
}

/// Constructor for a new subdetector element
DetElement::DetElement(DetElement det_parent, const std::string& det_name, int det_id) {
  assign(new Object(det_name,det_id), det_name, det_parent.type());
  ptr()->id = det_id;
  det_parent.add(*this);
}

/// Add an extension object to the detector element
void* DetElement::addExtension(ExtensionEntry* e) const  {
  return access()->addExtension(e->hash64(), e);
}

/// Access an existing extension object from the detector element
void* DetElement::extension(unsigned long long int k, bool alert) const {
  return access()->extension(k, alert);
}

/// Internal call to extend the detector element with an arbitrary structure accessible by the type
void DetElement::i_addUpdateCall(unsigned int callback_type, const Callback& callback)  const  {
  access()->updateCalls.emplace_back(callback,callback_type);
}

/// Remove callback from object
void DetElement::removeAtUpdate(unsigned int typ, void* pointer)  const {
  access()->removeAtUpdate(typ,pointer);
}

/// Access to the full path to the placed object
const std::string& DetElement::placementPath() const {
  Object* o = ptr();
  if ( o ) {
    if (o->placementPath.empty()) {
      o->placementPath = detail::tools::placementPath(*this);
    }
    return o->placementPath;
  }
  return s_empty_string;
}

/// Access detector type (structure, tracker, calorimeter, etc.).
std::string DetElement::type() const {
  return m_element ? m_element->GetTitle() : "";
}

/// Set the type of the sensitive detector
DetElement& DetElement::setType(const std::string& typ) {
  access()->SetTitle(typ.c_str());
  return *this;
}

unsigned int DetElement::typeFlag() const {
  return m_element ? m_element->typeFlag :  0 ;
}

/// Set the type of the sensitive detector
DetElement& DetElement::setTypeFlag(unsigned int types) {
  access()->typeFlag = types ;
  return *this;
}

namespace {
  static void make_path(DetElement::Object* o)   {
    DetElement par = o->parent;
    if ( par.isValid() )  {
      o->path = par.path() + "/" + o->name;
      if ( o->level < 0 ) o->level = par.level() + 1;
    }
    else {
      o->path = "/" + o->name;
      o->level = 0;
    }
    o->key = dd4hep::detail::hash32(o->path);
  }
}

/// Access hash key of this detector element (Only valid once geometry is closed!)
unsigned int DetElement::key()  const   {
  Object* o = ptr();
  if ( o )  {
    if ( o->key != 0 )
      return o->key;
    make_path(o);
    return o->key;
  }
  return 0;
}

/// Access the hierarchical level of the detector element
int DetElement::level()  const   {
  Object* o = ptr();
  if ( o )  {
    if ( o->level >= 0 )
      return o->level;
    make_path(o);
    return o->level;
  }
  return -1;
}

/// Access the full path of the detector element
const std::string& DetElement::path() const {
  Object* o = ptr();
  if ( o ) {
    if ( !o->path.empty() )
      return o->path;
    make_path(o);
    return o->path;
  }
  return s_empty_string;
}

int DetElement::id() const {
  return access()->id;
}

bool DetElement::combineHits() const {
  return access()->combineHits != 0;
}

DetElement& DetElement::setCombineHits(bool value, SensitiveDetector& sens) {
  access()->combineHits = value;
  if (sens.isValid())
    sens.setCombineHits(value);
  return *this;
}

/// Access to the alignment information
Alignment DetElement::nominal() const   {
  Object* o = access();
  if ( !o->nominal.isValid() )   {
    o->nominal = AlignmentCondition("nominal");
    o->nominal->values().detector = *this;
    //o->flag |= Object::HAVE_WORLD_TRAFO;
    //o->flag |= Object::HAVE_PARENT_TRAFO;
    dd4hep::detail::tools::computeIdeal(o->nominal);
  }
  return o->nominal;
}

/// Access to the survey alignment information
Alignment DetElement::survey() const  {
  Object* o = access();
  if ( !o->survey.isValid() )   {
    o->survey = AlignmentCondition("survey");
    dd4hep::detail::tools::copy(nominal(), o->survey);
  }
  return o->survey;
}

const DetElement::Children& DetElement::children() const {
  return access()->children;
}

/// Access to individual children by name
DetElement DetElement::child(const std::string& child_name) const {
  if (isValid()) {
    const Children& c = ptr()->children;
    Children::const_iterator i = c.find(child_name);
    if ( i != c.end() ) return (*i).second;
    throw std::runtime_error("dd4hep: DetElement::child Unknown child with name: "+child_name);
  }
  throw std::runtime_error("dd4hep: DetElement::child: Self is not defined [Invalid Handle]");
}

/// Access to individual children by name. Have option to not throw an exception
DetElement DetElement::child(const std::string& child_name, bool throw_if_not_found) const {
  if (isValid()) {
    const Children& c = ptr()->children;
    Children::const_iterator i = c.find(child_name);
    if ( i != c.end() ) return (*i).second;
    if ( throw_if_not_found )   {
      throw std::runtime_error("dd4hep: DetElement::child Unknown child with name: "+child_name);
    }
  }
  if ( throw_if_not_found )   {
    throw std::runtime_error("dd4hep: DetElement::child: Self is not defined [Invalid Handle]");
  }
  return DetElement();
}

/// Access to the detector elements's parent
DetElement DetElement::parent() const {
  Object* o = ptr();
  return (o) ? o->parent : DetElement();
}

/// Access to the world object. Only possible once the geometry is closed.
DetElement DetElement::world()  const   {
  Object* o = ptr();
  return (o) ? o->world() : World();
}

/// Simple checking routine
void DetElement::check(bool cond, const std::string& msg) const {
  if (cond) {
    throw std::runtime_error("dd4hep: " + msg);
  }
}

/// Add a new child subdetector element
DetElement& DetElement::add(DetElement sdet) {
  if (isValid()) {
    auto r = object<Object>().children.emplace(sdet.name(), sdet);
    if (r.second) {
      sdet.access()->parent = *this;
      return *this;
    }
    except("dd4hep",
           "DetElement::add: Element %s is already present in path %s [Double-Insert]",
           sdet.name(), this->path().c_str());
  }
  except("dd4hep", "DetElement::add: Self is not defined [Invalid Handle]");
  throw std::runtime_error("dd4hep: DetElement::add");
}

/// Clone (Deep copy) the DetElement structure
DetElement DetElement::clone(int flg) const   {
  Object* o = access();
  Object* n = o->clone(o->id, flg);
  n->SetName(o->GetName());
  n->SetTitle(o->GetTitle());
  return n;
}

DetElement DetElement::clone(const std::string& new_name) const {
  return clone(new_name, access()->id);
}

DetElement DetElement::clone(const std::string& new_name, int new_id) const {
  Object* o = access();
  Object* n = o->clone(new_id, COPY_PLACEMENT);
  n->SetName(new_name.c_str());
  n->SetTitle(o->GetTitle());
  return n;
}

std::pair<DetElement,Volume> DetElement::reflect(const std::string& new_name) const {
  return reflect(new_name, access()->id);
}

std::pair<DetElement,Volume> DetElement::reflect(const std::string& new_name, int new_id) const {
  return reflect(new_name, new_id, SensitiveDetector(0));
}

std::pair<DetElement,Volume> DetElement::reflect(const std::string& new_name, int new_id, SensitiveDetector sd) const {
  if ( placement().isValid() )   {
    return m_element->reflect(new_name, new_id, sd);
  }
  except("DetElement","reflect: Only placed DetElement objects can be reflected: %s",
         path().c_str());
  return std::make_pair(DetElement(),Volume());
}

/// Access to the ideal physical volume of this detector element
PlacedVolume DetElement::idealPlacement() const    {
  if (isValid()) {
    Object& o = object<Object>();
    return o.idealPlace;
  }
  return PlacedVolume();
}

/// Access to the physical volume of this detector element
PlacedVolume DetElement::placement() const {
  if (isValid()) {
    Object& o = object<Object>();
    return o.placement;
  }
  return PlacedVolume();
}

/// Set the physical volumes of the detector element
DetElement& DetElement::setPlacement(const PlacedVolume& pv) {
  if (pv.isValid()) {
    Object* o = access();
    o->placement = pv;
    if ( !o->idealPlace.isValid() )  {
      o->idealPlace = pv;
    }
    return *this;
  }
  except("dd4hep", "DetElement::setPlacement: Placement is not defined [Invalid Handle]");
  throw std::runtime_error("dd4hep: DetElement::add");
}

/// The cached VolumeID of this subdetector element
dd4hep::VolumeID DetElement::volumeID() const   {
  if (isValid()) {
    return object<Object>().volumeID;
  }
  return 0;
}

/// Access to the logical volume of the placements (all daughters have the same!)
Volume DetElement::volume() const {
  return access()->placement.volume();
}

/// Access to the shape of the detector element's placement
Solid DetElement::solid() const    {
  return volume()->GetShape();
}

DetElement& DetElement::setVisAttributes(const Detector& description, const std::string& nam, const Volume& vol) {
  vol.setVisAttributes(description, nam);
  return *this;
}

DetElement& DetElement::setRegion(const Detector& description, const std::string& nam, const Volume& vol) {
  if (!nam.empty()) {
    vol.setRegion(description.region(nam));
  }
  return *this;
}

DetElement& DetElement::setLimitSet(const Detector& description, const std::string& nam, const Volume& vol) {
  if (!nam.empty()) {
    vol.setLimitSet(description.limitSet(nam));
  }
  return *this;
}

DetElement& DetElement::setAttributes(const Detector& description,
                                      const Volume& vol,
                                      const std::string& region,
                                      const std::string& limits,
                                      const std::string& vis)
{
  return setRegion(description, region, vol).setLimitSet(description, limits, vol).setVisAttributes(description, vis, vol);
}

/// Constructor
SensitiveDetector::SensitiveDetector(const std::string& nam, const std::string& typ) {
  /*
    <calorimeter ecut="0" eunit="MeV" hits_collection="EcalEndcapHits" name="EcalEndcap" verbose="0">
    <global_grid_xy grid_size_x="3.5" grid_size_y="3.5"/>
    <idspecref ref="EcalEndcapHits"/>
    </calorimeter>
  */
  assign(new Object(nam), nam, typ);
  object<Object>().ecut = 0e0;
  object<Object>().verbose = 0;
}

/// Set the type of the sensitive detector
SensitiveDetector& SensitiveDetector::setType(const std::string& typ) {
  access()->SetTitle(typ.c_str());
  return *this;
}

/// Access the type of the sensitive detector
std::string SensitiveDetector::type() const {
  return m_element ? m_element->GetTitle() : s_empty_string;
}

/// Assign the IDDescriptor reference
SensitiveDetector& SensitiveDetector::setReadout(Readout ro) {
  access()->readout = ro;
  return *this;
}

/// Assign the IDDescriptor reference
Readout SensitiveDetector::readout() const {
  return access()->readout;
}

/// Assign the IDDescriptor reference
IDDescriptor SensitiveDetector::idSpec() const {
  return readout().idSpec();
}

/// Set energy cut off
SensitiveDetector& SensitiveDetector::setEnergyCutoff(double value) {
  access()->ecut = value;
  return *this;
}

/// Access energy cut off
double SensitiveDetector::energyCutoff() const {
  return access()->ecut;
}

/// Assign the name of the hits collection
SensitiveDetector& SensitiveDetector::setHitsCollection(const std::string& collection) {
  access()->hitsCollection = collection;
  return *this;
}

/// Access the hits collection name
const std::string& SensitiveDetector::hitsCollection() const {
  return access()->hitsCollection;
}

/// Assign the name of the hits collection
SensitiveDetector& SensitiveDetector::setVerbose(bool value) {
  int v = value ? 1 : 0;
  access()->verbose = v;
  return *this;
}

/// Access flag to combine hist
bool SensitiveDetector::verbose() const {
  return access()->verbose == 1;
}

/// Assign the name of the hits collection
SensitiveDetector& SensitiveDetector::setCombineHits(bool value) {
  int v = value ? 1 : 0;
  access()->combineHits = v;
  return *this;
}

/// Access flag to combine hist
bool SensitiveDetector::combineHits() const {
  return access()->combineHits == 1;
}

/// Set the regional attributes to the sensitive detector
SensitiveDetector& SensitiveDetector::setRegion(Region reg) {
  access()->region = reg;
  return *this;
}

/// Access to the region setting of the sensitive detector (not mandatory)
Region SensitiveDetector::region() const {
  return access()->region;
}

/// Set the limits to the sensitive detector
SensitiveDetector& SensitiveDetector::setLimitSet(LimitSet ls) {
  access()->limits = ls;
  return *this;
}

/// Access to the limit set of the sensitive detector (not mandatory).
LimitSet SensitiveDetector::limits() const {
  return access()->limits;
}

/// Add an extension object to the detector element
void* SensitiveDetector::addExtension(unsigned long long int k,ExtensionEntry* e)  const
{
  return access()->addExtension(k,e);
}

/// Access an existing extension object from the detector element
void* SensitiveDetector::extension(unsigned long long int k) const {
  return access()->extension(k);
}

/// Access an existing extension object from the detector element
void* SensitiveDetector::extension(unsigned long long int k, bool alert) const {
  return access()->extension(k, alert);
}
