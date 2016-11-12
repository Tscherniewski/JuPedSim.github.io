/**
 * \file        Building.cpp
 * \date        Oct 1, 2014
 * \version     v0.7
 * \copyright   <2009-2015> Forschungszentrum Jülich GmbH. All rights reserved.
 *
 * \section License
 * This file is part of JuPedSim.
 *
 * JuPedSim is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * JuPedSim is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with JuPedSim. If not, see <http://www.gnu.org/licenses/>.
 *
 * \section Description
 *
 *
 **/


#include "Building.h"

#include "../geometry/SubRoom.h"
#include "../tinyxml/tinyxml.h"

#ifdef _SIMULATOR

#include "GeometryReader.h"
#include "../pedestrian/Pedestrian.h"
#include "../mpi/LCGrid.h"
#include "../routing/RoutingEngine.h"
#include "../pedestrian/PedDistributor.h"
#include "../IO/GeoFileParser.h"
#include "../hybrid/GeometryFromProtobufLoader.h"

#endif

//#undef _OPENMP

#ifdef _OPENMP

#include <omp.h>

#else
#define omp_get_thread_num()    0
#define omp_get_max_threads()   1
#endif

using namespace std;

Building::Building()
{
     _caption = "no_caption";
     _geometryFilename = "";
     _routingEngine = nullptr;
     _linkedCellGrid = nullptr;
     _savePathway = false;
}

#ifdef _SIMULATOR

Building::Building(const Configuration* configuration, PedDistributor& pedDistributor)
          :_configuration(configuration),
           _routingEngine(
                 configuration->GetRoutingEngine()),
           _caption("no_caption")
{
     _savePathway = false;
     _linkedCellGrid = nullptr;

#ifdef _JPS_AS_A_SERVICE
     if (_configuration->GetRunAsService()) {
           std::unique_ptr<GeometryFromProtobufLoader> parser(new GeometryFromProtobufLoader(_configuration));
           parser->LoadBuilding(this);
     }
     else
#endif
     {
           std::unique_ptr<GeoFileParser> parser(new GeoFileParser(_configuration));
           parser->LoadBuilding(this);
     }

     if (!InitGeometry()) {
          Log->Write("ERROR:\t could not initialize the geometry!");
          exit(EXIT_FAILURE);
     }



     //TODO: check whether traffic info can be loaded before InitGeometry if so call it in LoadBuilding instead and make
     //TODO: LoadTrafficInfo private [gl march '16]


     if (!pedDistributor.Distribute(this)) {
          Log->Write("ERROR:\t could not distribute the pedestrians");
          exit(EXIT_FAILURE);
     }

     InitGrid();

     if (!_routingEngine->Init(this)) {
          Log->Write("ERROR:\t could not initialize the routers!");
          exit(EXIT_FAILURE);
     }

     if (!SanityCheck()) {
          Log->Write("ERROR:\t There are sanity errors in the geometry file");
          exit(EXIT_FAILURE);
     }

//     SaveGeometry("/home/laemmel/Desktop/geo.xml");
}

#endif

Building::~Building()
{
     //
     // for (int i = 0; i < GetNumberOfRooms(); i++)
     //    delete _rooms[i];

#ifdef _SIMULATOR
     for (unsigned int p = 0; p<_allPedestians.size(); p++) {
          delete _allPedestians[p];
     }
     _allPedestians.clear();
     delete _linkedCellGrid;
#endif

     if (_pathWayStream.is_open())
          _pathWayStream.close();

     for (map<int, Crossing*>::const_iterator iter = _crossings.begin();
          iter!=_crossings.end(); ++iter) {
          delete iter->second;
     }
     for (map<int, Transition*>::const_iterator iter = _transitions.begin();
          iter!=_transitions.end(); ++iter) {
          delete iter->second;
     }
     for (map<int, Hline*>::const_iterator iter = _hLines.begin();
          iter!=_hLines.end(); ++iter) {
          delete iter->second;
     }
     for (map<int, Goal*>::const_iterator iter = _goals.begin();
          iter!=_goals.end(); ++iter) {
          delete iter->second;
     }
}

///************************************************************
// setters
// ************************************************************/
void Building::SetCaption(const std::string& s)
{
     _caption = s;
}

/*************************************************************
 getters
 ************************************************************/

string Building::GetCaption() const
{
     return _caption;
}

RoutingEngine* Building::GetRoutingEngine() const
{
     return _configuration->GetRoutingEngine().get();
}

int Building::GetNumberOfRooms() const
{
     return (int) _rooms.size();
}

int Building::GetNumberOfGoals() const
{
     return (int) (_transitions.size()+_hLines.size()+_crossings.size());
}

const std::map<int, std::shared_ptr<Room> >& Building::GetAllRooms() const
{
     return _rooms;
}

Room* Building::GetRoom(int index) const
{
     //todo: obsolete since the check is done by .at()
     if (_rooms.count(index)==0) {
          Log->Write("ERROR: Wrong 'index' in CBuiling::GetRoom() Room ID: %d size: %d", index, _rooms.size());
          Log->Write("\tControl your rooms ID and make sure they are in the order 0, 1, 2,.. ");
          return nullptr;
     }
     //return _rooms[index];
     return _rooms.at(index).get();
}

LCGrid* Building::GetGrid() const
{
     return _linkedCellGrid;
}

void Building::AddRoom(Room* room)
{
     _rooms[room->GetID()] = std::shared_ptr<Room>(room);
}

void Building::AddSurroundingRoom()
{
     Log->Write("INFO: \tAdding the room 'outside' ");
     // first look for the geometry boundaries
     double x_min = FLT_MAX;
     double x_max = -FLT_MAX;
     double y_min = FLT_MAX;
     double y_max = -FLT_MAX;
     //finding the bounding of the grid
     // and collect the pedestrians

     for (auto&& itr_room: _rooms) {
          for (auto&& itr_subroom: itr_room.second->GetAllSubRooms()) {
               for (auto&& wall:itr_subroom.second->GetAllWalls()) {
                    double x1 = wall.GetPoint1()._x;
                    double y1 = wall.GetPoint1()._y;
                    double x2 = wall.GetPoint2()._x;
                    double y2 = wall.GetPoint2()._y;

                    double xmax = (x1>x2) ? x1 : x2;
                    double xmin = (x1>x2) ? x2 : x1;
                    double ymax = (y1>y2) ? y1 : y2;
                    double ymin = (y1>y2) ? y2 : y1;

                    x_min = (xmin<=x_min) ? xmin : x_min;
                    x_max = (xmax>=x_max) ? xmax : x_max;
                    y_max = (ymax>=y_max) ? ymax : y_max;
                    y_min = (ymin<=y_min) ? ymin : y_min;
               }
          }
     }

     for (auto&& itr_goal:_goals) {
          for (auto&& wall: itr_goal.second->GetAllWalls()) {
               double x1 = wall.GetPoint1()._x;
               double y1 = wall.GetPoint1()._y;
               double x2 = wall.GetPoint2()._x;
               double y2 = wall.GetPoint2()._y;

               double xmax = (x1>x2) ? x1 : x2;
               double xmin = (x1>x2) ? x2 : x1;
               double ymax = (y1>y2) ? y1 : y2;
               double ymin = (y1>y2) ? y2 : y1;

               x_min = (xmin<=x_min) ? xmin : x_min;
               x_max = (xmax>=x_max) ? xmax : x_max;
               y_max = (ymax>=y_max) ? ymax : y_max;
               y_min = (ymin<=y_min) ? ymin : y_min;
          }
     }
     //make the grid slightly larger.
     x_min = x_min-10.0;
     x_max = x_max+10.0;
     y_min = y_min-10.0;
     y_max = y_max+10.0;

     SubRoom* bigSubroom = new NormalSubRoom();
     bigSubroom->SetRoomID((int) _rooms.size());
     bigSubroom->SetSubRoomID(0); // should be the single subroom
     bigSubroom->AddWall(Wall(Point(x_min, y_min), Point(x_min, y_max)));
     bigSubroom->AddWall(Wall(Point(x_min, y_max), Point(x_max, y_max)));
     bigSubroom->AddWall(Wall(Point(x_max, y_max), Point(x_max, y_min)));
     bigSubroom->AddWall(Wall(Point(x_max, y_min), Point(x_min, y_min)));

     Room* bigRoom = new Room();
     bigRoom->AddSubRoom(bigSubroom);
     bigRoom->SetCaption("outside");
     bigRoom->SetID((int) _rooms.size());
     AddRoom(bigRoom);
}

bool Building::InitGeometry()
{
     Log->Write("INFO: \tInit Geometry");

     for (auto&& itr_room: _rooms) {
          for (auto&& itr_subroom: itr_room.second->GetAllSubRooms()) {
               //create a close polyline out of everything
               vector<Line*> goals = vector<Line*>();

               //  collect all crossings
               for (auto&& cros:itr_subroom.second->GetAllCrossings()) {
                    goals.push_back(cros);
               }
               //collect all transitions
               for (auto&& trans:itr_subroom.second->GetAllTransitions()) {
                    goals.push_back(trans);
               }
               // initialize the poly
               if (!itr_subroom.second->ConvertLineToPoly(goals))
                    return false;
               itr_subroom.second->CalculateArea();

               //do the same for the obstacles that are closed
               for (auto&& obst:itr_subroom.second->GetAllObstacles()) {
                    //if (obst->GetClosed() == 1)
                    if (!obst->ConvertLineToPoly())
                         return false;
               }

               double minElevation = FLT_MAX;
               double maxElevation = -FLT_MAX;
               for (auto&& wall:itr_subroom.second->GetAllWalls()) {
                    const Point& P1 = wall.GetPoint1();
                    const Point& P2 = wall.GetPoint2();
                    if (minElevation>itr_subroom.second->GetElevation(P1)) {
                         minElevation = itr_subroom.second->GetElevation(P1);
                    }

                    if (maxElevation<itr_subroom.second->GetElevation(P1)) {
                         maxElevation = itr_subroom.second->GetElevation(P1);
                    }

                    if (minElevation>itr_subroom.second->GetElevation(P2)) {
                         minElevation = itr_subroom.second->GetElevation(P2);
                    }

                    if (maxElevation<itr_subroom.second->GetElevation(P2)) {
                         maxElevation = itr_subroom.second->GetElevation(P2);
                    }
               }
               itr_subroom.second->SetMaxElevation(maxElevation);
               itr_subroom.second->SetMinElevation(minElevation);
          }
     }


     // look and save the neighbor subroom for improving the runtime
     // that information is already present in the crossing/transitions

     for (const auto& cross: _crossings) {
          SubRoom* s1 = cross.second->GetSubRoom1();
          SubRoom* s2 = cross.second->GetSubRoom2();
          if (s1) s1->AddNeighbor(s2);
          if (s2) s2->AddNeighbor(s1);
     }

     for (const auto& trans: _transitions) {
          SubRoom* s1 = trans.second->GetSubRoom1();
          SubRoom* s2 = trans.second->GetSubRoom2();
          if (s1) s1->AddNeighbor(s2);
          if (s2) s2->AddNeighbor(s1);
     }

     Log->Write("INFO: \tInit Geometry successful!!!\n");

     return true;
}

const string& Building::GetProjectFilename() const
{
     return _configuration->GetProjectFile();
}

const string& Building::GetProjectRootDir() const
{
     return _configuration->GetProjectRootDir();
}

const std::string& Building::GetGeometryFilename() const
{
     return _configuration->GetGeometryFile();
}

void Building::WriteToErrorLog() const
{
     Log->Write("GEOMETRY: ");
     for (int i = 0; i<GetNumberOfRooms(); i++) {
          Room* r = GetRoom(i);
          r->WriteToErrorLog();
     }
     Log->Write("ROUTING: ");

     for (map<int, Crossing*>::const_iterator iter = _crossings.begin();
          iter!=_crossings.end(); ++iter) {
          iter->second->WriteToErrorLog();
     }
     for (map<int, Transition*>::const_iterator iter = _transitions.begin();
          iter!=_transitions.end(); ++iter) {
          iter->second->WriteToErrorLog();
     }
     for (map<int, Hline*>::const_iterator iter = _hLines.begin();
          iter!=_hLines.end(); ++iter) {
          iter->second->WriteToErrorLog();
     }
     Log->Write("\n");
}

Room* Building::GetRoom(string caption) const
{
     for (const auto& it: _rooms) {
          if (it.second->GetCaption()==caption)
               return it.second.get();
     }
     Log->Write("ERROR: Room not found with caption "+caption);
     //return NULL;
     exit(EXIT_FAILURE);
}

bool Building::AddCrossing(Crossing* line)
{
     if (_crossings.count(line->GetID())!=0) {
          char tmp[CLENGTH];
          sprintf(tmp,
                    "ERROR: Duplicate index for crossing found [%d] in Routing::AddCrossing()",
                    line->GetID());
          Log->Write(tmp);
          exit(EXIT_FAILURE);
     }
     _crossings[line->GetID()] = line;
     return true;
}

bool Building::AddTransition(Transition* line)
{
     if (_transitions.count(line->GetID())!=0) {
          char tmp[CLENGTH];
          sprintf(tmp,
                    "ERROR: Duplicate index for transition found [%d] in Routing::AddTransition()",
                    line->GetID());
          Log->Write(tmp);
          exit(EXIT_FAILURE);
     }
     _transitions[line->GetID()] = line;
     return true;
}

bool Building::AddHline(Hline* line)
{
     if (_hLines.count(line->GetID())!=0) {
          // check if the lines are identical
          Hline* ori = _hLines[line->GetID()];
          if (ori->operator==(*line)) {
               Log->Write("INFO: \tSkipping identical hlines with ID [%d]", line->GetID());
               return false;
          }
          else {
               Log->Write(
                         "ERROR: Duplicate index for hlines found [%d] in Routing::AddHline(). You have [%d] hlines",
                         line->GetID(), _hLines.size());
               exit(EXIT_FAILURE);
          }
     }
     _hLines[line->GetID()] = line;
     return true;
}

bool Building::AddGoal(Goal* goal)
{
     if (_goals.count(goal->GetId())!=0) {
          Log->Write(
                    "ERROR: Duplicate index for goal found [%d] in Routing::AddGoal()",
                    goal->GetId());
          exit(EXIT_FAILURE);
     }
     _goals[goal->GetId()] = goal;
     return true;
}

const map<int, Crossing*>& Building::GetAllCrossings() const
{
     return _crossings;
}

const map<int, Transition*>& Building::GetAllTransitions() const
{
     return _transitions;
}

const map<int, Hline*>& Building::GetAllHlines() const
{
     return _hLines;
}

const map<int, Goal*>& Building::GetAllGoals() const
{
     return _goals;
}

Transition* Building::GetTransition(string caption) const
{
     //eventually
     map<int, Transition*>::const_iterator itr;
     for (itr = _transitions.begin(); itr!=_transitions.end(); ++itr) {
          if (itr->second->GetCaption()==caption)
               return itr->second;
     }

     Log->Write("WARNING: No Transition with Caption: "+caption);
     exit(EXIT_FAILURE);
}

Transition* Building::GetTransition(int ID) const //ar.graf: added const 2015-12-10
{
     if (_transitions.count(ID)==1) {
          return _transitions.at(ID);
     }
     else {
          if (ID==-1)
               return NULL;
          else {
               Log->Write(
                         "ERROR: I could not find any transition with the 'ID' [%d]. You have defined [%d] transitions",
                         ID, _transitions.size());
               exit(EXIT_FAILURE);
          }
     }
}

Crossing* Building::GetCrossing(int ID)
{
     if (_crossings.count(ID)==1) {
          return _crossings[ID];
     }
     else {
          if (ID==-1)
               return NULL;
          else {
               Log->Write(
                         "ERROR: I could not find any crossing with the 'ID' [%d]. You have defined [%d] transitions",
                         ID, _crossings.size());
               exit(EXIT_FAILURE);
          }
     }
}

Goal* Building::GetFinalGoal(int ID) const
{
     if (_goals.count(ID)==1) {
          return _goals.at(ID);
     }
     else {
          if (ID==-1)
               return NULL;
          else {
               Log->Write(
                         "ERROR: I could not find any goal with the 'ID' [%d]. You have defined [%d] goals",
                         ID, _goals.size());
               exit(EXIT_FAILURE);
          }
     }
}

Crossing* Building::GetTransOrCrossByName(string caption) const
{
     {
          //eventually
          map<int, Transition*>::const_iterator itr;
          for (itr = _transitions.begin(); itr!=_transitions.end(); ++itr) {
               if (itr->second->GetCaption()==caption)
                    return itr->second;
          }
     }
     {
          //finally the  crossings
          map<int, Crossing*>::const_iterator itr;
          for (itr = _crossings.begin(); itr!=_crossings.end(); ++itr) {
               if (itr->second->GetCaption()==caption)
                    return itr->second;
          }
     }

     Log->Write("WARNING: No Transition or Crossing with Caption: "+caption);
     return NULL;
}

Hline* Building::GetTransOrCrossByUID(int id) const
{
     {
          //eventually transitions
          map<int, Transition*>::const_iterator itr;
          for (itr = _transitions.begin(); itr!=_transitions.end(); ++itr) {
               if (itr->second->GetUniqueID()==id)
                    return itr->second;
          }
     }
     {
          //then the  crossings
          map<int, Crossing*>::const_iterator itr;
          for (itr = _crossings.begin(); itr!=_crossings.end(); ++itr) {
               if (itr->second->GetUniqueID()==id)
                    return itr->second;
          }
     }
     {
          //finally the  hlines
          for (auto itr = _hLines.begin(); itr!=_hLines.end(); ++itr) {
               if (itr->second->GetUniqueID()==id)
                    return itr->second;
          }
     }
     Log->Write("ERROR: No Transition or Crossing or hline with ID %d: ", id);
     return NULL;
}

SubRoom* Building::GetSubRoomByUID(int uid) const
{
     for (auto&& itr_room: _rooms) {
          for (auto&& itr_subroom: itr_room.second->GetAllSubRooms()) {
               if (itr_subroom.second->GetUID()==uid)
                    return itr_subroom.second.get();
          }
     }
     Log->Write("ERROR:\t No subroom exits with the unique id %d", uid);
     return NULL;
}

//bool Building::IsVisible(Line* l1, Line* l2, bool considerHlines)
//{
//
//     for(auto&& itr_room: _rooms)
//     {
//          for(auto&& itr_subroom: itr_room.second->GetAllSubRooms())
//          {
//               if(itr_subroom.second->IsVisible(l1,l2,considerHlines)==false) return false;
//          }
//     }
//     return true;
//}

bool Building::IsVisible(const Point& p1, const Point& p2, const std::vector<SubRoom*>& subrooms,
          bool considerHlines)
{
     //loop over all subrooms if none is provided
     if (subrooms.empty()) {
          for (auto&& itr_room: _rooms) {
               for (auto&& itr_subroom: itr_room.second->GetAllSubRooms()) {
                    if (!itr_subroom.second->IsVisible(p1, p2, considerHlines)) return false;
               }
          }
     }
     else {
          for (auto&& sub: subrooms) {
               if (sub && !sub->IsVisible(p1, p2, considerHlines)) return false;
          }
     }

     return true;
}

bool Building::Triangulate()
{
     Log->Write("INFO:\tTriangulating the geometry");
     for (auto&& itr_room: _rooms) {
          for (auto&& itr_subroom: itr_room.second->GetAllSubRooms()) {
               if (!itr_subroom.second->Triangulate())
                    return false;
          }
     }
     Log->Write("INFO:\tDone...");
     return true;
}

std::vector<Point> Building::GetBoundaryVertices() const
{
    double xMin=FLT_MAX;
    double yMin=FLT_MAX;
    double xMax=-FLT_MAX;
    double yMax=-FLT_MAX;
    for(auto&& itr_room: _rooms)
    {
         for(auto&& itr_subroom: itr_room.second->GetAllSubRooms())
         {
             const std::vector<Point> vertices = itr_subroom.second->GetPolygon();

             for (Point point:vertices)
             {
                 if (point._x>xMax)
                     xMax=point._x;
                 else if (point._x<xMin)
                     xMin=point._x;
                 if (point._y>yMax)
                     yMax=point._y;
                 else if (point._y<yMin)
                     yMin=point._y;
             }
         }

    }
    return std::vector<Point>{Point(xMin,yMin),Point(xMin,yMax),Point(xMax,yMax),Point(xMax,yMin)};
}

bool Building::SanityCheck()
{
     Log->Write("INFO: \tChecking the geometry for artifacts");
     bool status = true;

     //only for ffRouter
     return status;

     for(auto&& itr_room: _rooms)
     {
          for(auto&& itr_subroom: itr_room.second->GetAllSubRooms())
          {
               if (!itr_subroom.second->SanityCheck())
                    status = false;
          }
     }

     Log->Write("INFO: \t...Done!!!\n");
     return status;
}

#ifdef _SIMULATOR

void Building::UpdateGrid()
{
     _linkedCellGrid->Update(_allPedestians);
}

void Building::InitGrid()
{
     // first look for the geometry boundaries
     double x_min = FLT_MAX;
     double x_max = FLT_MIN;
     double y_min = FLT_MAX;
     double y_max = FLT_MIN;

     //finding the bounding of the grid
     // and collect the pedestrians
     for (auto&& itr_room: _rooms) {
          for (auto&& itr_subroom: itr_room.second->GetAllSubRooms()) {
               for (auto&& wall:itr_subroom.second->GetAllWalls()) {
                    double x1 = wall.GetPoint1()._x;
                    double y1 = wall.GetPoint1()._y;
                    double x2 = wall.GetPoint2()._x;
                    double y2 = wall.GetPoint2()._y;

                    double xmax = (x1>x2) ? x1 : x2;
                    double xmin = (x1>x2) ? x2 : x1;
                    double ymax = (y1>y2) ? y1 : y2;
                    double ymin = (y1>y2) ? y2 : y1;

                    x_min = (xmin<=x_min) ? xmin : x_min;
                    x_max = (xmax>=x_max) ? xmax : x_max;
                    y_max = (ymax>=y_max) ? ymax : y_max;
                    y_min = (ymin<=y_min) ? ymin : y_min;
               }
          }
     }

     double cellSize = _configuration->GetLinkedCellSize();
     //make the grid slightly larger.
     x_min = x_min-1*cellSize;
     x_max = x_max+1*cellSize;
     y_min = y_min-1*cellSize;
     y_max = y_max+1*cellSize;

     double boundaries[4] = {x_min, x_max, y_min, y_max};

     //no algorithms
     // the domain is made of a single cell
     if (cellSize==-1) {
          Log->Write("INFO: \tBrute Force will be used for neighborhoods query");
          if ((x_max-x_min)<(y_max-y_min)) {
               cellSize = (y_max-y_min);
          }
          else {
               cellSize = (x_max-x_min);
          }

     }
     else {
          Log->Write("INFO: \tInitializing the grid with cell size: %f ", cellSize);
     }

     //TODO: the number of pedestrian should be calculated using the capacity of the sources
     //int nped= Pedestrian::GetAgentsCreated() +  for src:sources  src->GetMaxAgents()

     _linkedCellGrid = new LCGrid(boundaries, cellSize, Pedestrian::GetAgentsCreated());
     _linkedCellGrid->ShallowCopy(_allPedestians);

     Log->Write("INFO: \tDone with Initializing the grid ");
}

void Building::DeletePedestrian(Pedestrian*& ped)
{
     vector<Pedestrian*>::iterator it;
     it = find(_allPedestians.begin(), _allPedestians.end(), ped);
     if (it==_allPedestians.end()) {
          Log->Write("\tERROR: \tPed not found with ID %d ", ped->GetID());
          // exit(EXIT_FAILURE);
          return;
     }
     else {
          // save the path history for this pedestrian before removing from the simulation
          if (_savePathway) {
               // string results;
               string path = (*it)->GetPath();
               vector<string> brokenpaths;
               StringExplode(path, ">", &brokenpaths);
               for (unsigned int i = 0; i<brokenpaths.size(); i++) {
                    vector<string> tags;
                    StringExplode(brokenpaths[i], ":", &tags);
                    string room = _rooms[atoi(tags[0].c_str())]->GetCaption();
                    string trans = GetTransition(atoi(tags[1].c_str()))->GetCaption();
                    //ignore crossings/hlines
                    if (trans!="")
                         _pathWayStream << room << " " << trans << endl;
               }

          }
     }
     //update the stats before deleting
//     Transition* trans =GetTransitionByUID(ped->GetExitIndex());
//     if(trans)
//     {
//          trans->IncreaseDoorUsage(1, ped->GetGlobalTime());
//          //this can happen if the pedesrians is pushed too hard
//          // or cant stop due to high velocity
//          // he will remain in the simulation in that case
//          //if(trans->IsOpen()==false) return;
//     }
     _allPedestians.erase(it);
     delete ped;
}

const vector<Pedestrian*>& Building::GetAllPedestrians() const
{
     return _allPedestians;
}

void Building::AddPedestrian(Pedestrian* ped)
{
     for (unsigned int p = 0; p<_allPedestians.size(); p++) {
          Pedestrian* ped1 = _allPedestians[p];
          if (ped->GetID()==ped1->GetID()) {
               cout << "Pedestrian already in the room ??? " << ped->GetID() << endl;
               return;
          }
     }
     _allPedestians.push_back(ped);
}

void Building::GetPedestrians(int room, int subroom, std::vector<Pedestrian*>& peds) const
{
     //for(unsigned int p = 0;p<_allPedestians.size();p++){
     //     Pedestrian* ped=_allPedestians[p];

     for (auto&& ped : _allPedestians) {
          if ((room==ped->GetRoomID()) && (subroom==ped->GetSubRoomID())) {
               peds.push_back(ped);
          }
     }
}

//obsolete
void Building::InitSavePedPathway(const string& filename)
{
     _pathWayStream.open(filename.c_str());
     _savePathway = true;

     if (_pathWayStream.is_open()) {
          Log->Write("#INFO:\tsaving pedestrian paths to [ "+filename+" ]");
          _pathWayStream << "##pedestrian ways" << endl;
          _pathWayStream << "#nomenclature roomid  caption" << endl;
          //              for (unsigned int r=0;r< pRooms.size();r++){
          //                      Room* room= GetRoom(r);
          //                      const vector<int>& goals=room->GetAllTransitionsIDs();
          //
          //                      for(unsigned int g=0;g<goals.size();g++){
          //                              int exitid=goals[g];
          //                              string exit_caption=pRouting->GetGoal(exitid)->GetCaption();
          //                              PpathWayStream<<exitid<<" "<<exit_caption<<endl;
          //                      }
          //              }
          //
          _pathWayStream << "#data room exit_id" << endl;
     }
     else {
          Log->Write("#INFO:\t Unable to open [ "+filename+" ]");
          Log->Write("#INFO:\t saving to stdout");

     }
}

void Building::StringExplode(string str, string separator,
          vector<string>* results)
{
     size_t found;
     found = str.find_first_of(separator);
     while (found!=string::npos) {
          if (found>0) {
               results->push_back(str.substr(0, found));
          }
          str = str.substr(found+1);
          found = str.find_first_of(separator);
     }
     if (str.length()>0) {
          results->push_back(str);
     }
}

Pedestrian* Building::GetPedestrian(int pedID) const
{
     for (unsigned int p = 0; p<_allPedestians.size(); p++) {
          Pedestrian* ped = _allPedestians[p];
          if (ped->GetID()==pedID) {
               return ped;
          }
     }

     return NULL;
}

Transition* Building::GetTransitionByUID(int uid) const
{

     for (auto&& trans: _transitions) {
          if (trans.second->GetUniqueID()==uid)
               return trans.second;
     }
     return nullptr;
}

//bool Building::SaveGeometry(const std::string& filename)
//{
//     std::stringstream geometry;
//
//     //write the header
//     geometry << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>" << endl;
//     geometry << "<geometry version=\"0.5\" caption=\"second life\" unit=\"m\"\n "
//               " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n  "
//               " xsi:noNamespaceSchemaLocation=\"http://134.94.2.137/jps_geoemtry.xsd\">" << endl << endl;
//
//     //write the rooms
//     geometry << "<rooms>" << endl;
//     for (auto&& itroom : _rooms) {
//          auto&& room = itroom.second;
//          geometry << "\t<room id =\"" << room->GetID() << "\" caption =\"" << room->GetCaption() << "\">" << endl;
//          for (auto&& itr_sub : room->GetAllSubRooms()) {
//               auto&& sub = itr_sub.second;
//               const double* plane = sub->GetPlaneEquation();
//               geometry << "\t\t<subroom id =\"" << sub->GetSubRoomID()
//                         << "\" closed=\"" << 0
//                         << "\" class=\"" << sub->GetType()
//                         << "\" A_x=\"" << plane[0]
//                         << "\" B_y=\"" << plane[1]
//                         << "\" C_z=\"" << plane[2] << "\">" << endl;
//
//               for (auto&& wall : sub->GetAllWalls()) {
//                    const Point& p1 = wall.GetPoint1();
//                    const Point& p2 = wall.GetPoint2();
//
//                    geometry << "\t\t\t<polygon caption=\"wall\" type=\"" << wall.GetType() << "\">" << endl
//                              << "\t\t\t\t<vertex px=\"" << p1._x << "\" py=\"" << p1._y << "\"/>" << endl
//                              << "\t\t\t\t<vertex px=\"" << p2._x << "\" py=\"" << p2._y << "\"/>" << endl
//                              << "\t\t\t</polygon>" << endl;
//               }
//
//               if (sub->GetType()=="stair") {
//                    const Point& up = ((Stair*) sub.get())->GetUp();
//                    const Point& down = ((Stair*) sub.get())->GetDown();
//                    geometry << "\t\t\t<up px=\"" << up._x << "\" py=\"" << up._y << "\"/>" << endl;
//                    geometry << "\t\t\t<down px=\"" << down._x << "\" py=\"" << down._y << "\"/>" << endl;
//               }
//
//               geometry << "\t\t</subroom>" << endl;
//          }
//
//          //write the crossings
//          geometry << "\t\t<crossings>" << endl;
//          for (auto const& mapcross : _crossings) {
//               Crossing* cross = mapcross.second;
//
//               //only write the crossings in this rooms
//               if (cross->GetRoom1()->GetID()!=room->GetID()) continue;
//
//               const Point& p1 = cross->GetPoint1();
//               const Point& p2 = cross->GetPoint2();
//
//               geometry << "\t<crossing id =\"" << cross->GetID()
//                         << "\" subroom1_id=\"" << cross->GetSubRoom1()->GetSubRoomID()
//                         << "\" subroom2_id=\"" << cross->GetSubRoom2()->GetSubRoomID() << "\">" << endl;
//
//               geometry << "\t\t<vertex px=\"" << p1._x << "\" py=\"" << p1._y << "\"/>" << endl
//                         << "\t\t<vertex px=\"" << p2._x << "\" py=\"" << p2._y << "\"/>" << endl
//                         << "\t</crossing>" << endl;
//          }
//          geometry << "\t\t</crossings>" << endl;
//          geometry << "\t</room>" << endl;
//     }
//
//     geometry << "</rooms>" << endl;
//
//     //write the transitions
//     geometry << "<transitions>" << endl;
//
//     for (auto const& maptrans : _transitions) {
//          Transition* trans = maptrans.second;
//          const Point& p1 = trans->GetPoint1();
//          const Point& p2 = trans->GetPoint2();
//          int room2_id = -1;
//          int subroom2_id = -1;
//          if (trans->GetRoom2()) {
//               room2_id = trans->GetRoom2()->GetID();
//               subroom2_id = trans->GetSubRoom2()->GetSubRoomID();
//          }
//
//          geometry << "\t<transition id =\"" << trans->GetID()
//                    << "\" caption=\"" << trans->GetCaption()
//                    << "\" type=\"" << trans->GetType()
//                    << "\" room1_id=\"" << trans->GetRoom1()->GetID()
//                    << "\" subroom1_id=\"" << trans->GetSubRoom1()->GetSubRoomID()
//                    << "\" room2_id=\"" << room2_id
//                    << "\" subroom2_id=\"" << subroom2_id << "\">" << endl;
//
//          geometry << "\t\t<vertex px=\"" << p1._x << "\" py=\"" << p1._y << "\"/>" << endl
//                    << "\t\t<vertex px=\"" << p2._x << "\" py=\"" << p2._y << "\"/>" << endl
//                    << "\t</transition>" << endl;
//
//     }
//
//     geometry << "</transitions>" << endl;
//     geometry << "</geometry>" << endl;
//     //write the routing file
//
//     //cout<<endl<<geometry.str()<<endl;
//
//     ofstream geofile(filename);
//     if (geofile.is_open()) {
//          geofile << geometry.str();
//          Log->Write("INFO:\tfile saved to %s", filename.c_str());
//     }
//     else {
//          Log->Write("ERROR:\tunable to save the geometry to %s", filename.c_str());
//          return false;
//     }
//
//     return true;
//}

#endif // _SIMULATOR

