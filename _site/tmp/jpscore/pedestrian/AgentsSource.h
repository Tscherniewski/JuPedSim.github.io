/**
 * \file        AgentSource.h
 * \date        Apr 04, 2015
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
 * This class is responsible for materialising agent in a given location at a given frequency.
 * The optimal position where to put the agents is given by various algorithms, for instance
 * the Voronoi algorithm or the Mitchell Best candidate algorithm.
 *
 **/

#ifndef AGENTSOURCE_H_
#define AGENTSOURCE_H_

#include <string>
#include <vector>
#include <memory>

class Pedestrian;
class OutputHandler;
class StartDistribution;
class Building;

// external variables
extern OutputHandler* Log;


class AgentsSource
{
public:
     /**
      * Constructor
      */
      AgentsSource(int id, const std::string& caption,int max_agents,int group_id,int frequency, bool greedy);

     /**
      * Destructor
      */
     virtual ~AgentsSource();

     /**
      * Add a new agent to this source
      * @param ped
      */
     void AddToPool(Pedestrian* ped);

     /**
      * Generate a number of agents, based on the frequency given in the constructor.
      * No agents are generated if the the maximum (_maxAgents) is reached.
      * @see _maxAgents
      * @param ped
      */
     void GenerateAgentsAndAddToPool(int count, Building* building);

     /**
      * Generate agents, but do not add them to the pool
      * @param ped, the container for the agents
      * @param count, the number of agents to generate
      * @param building, a pointer to the building object
      */
     void GenerateAgents(std::vector<Pedestrian*>& peds, int count, Building* building);

     /**
      * Generate count agents and save them in the vector
      * @param ped, the container for the agents
      * @param count, the number of agents to generate
      */
     void RemoveAgentsFromPool(std::vector<Pedestrian*>& peds, int count);

     /**
      * Add the agents to the pool. This might be important in the case the removed agents could not
      * be placed correctly. They can be requeued using this function.
      * @param peds
      */
     void AddAgentsToPool(std::vector<Pedestrian*>& peds);

     /**
      * @return the number of agents remaining
      */
     int GetPoolSize() const;

     /**
      * Print relevant information
      */
     void Dump() const;

     int GetAgentsGenerated() const;
     void SetAgentsGenerated(int agentsGenerated);
     const double* GetBoundaries() const;
     void Setboundaries(double * bounds);
     const std::string& GetCaption() const;
     int GetFrequency() const;
     int GetGroupId() const;
     int GetId() const;
     int GetMaxAgents() const;
     bool Greedy() const;
     void SetStartDistribution(std::shared_ptr<StartDistribution>);
     const std::shared_ptr<StartDistribution> GetStartDistribution() const;


private:
     int _id=-1;
     int _frequency=0;
     int _maxAgents=0;
     int _groupID=-1;
     std::string _caption="no caption";
     bool _greedy = false;
     int _agentsGenerated=0;
     double _boundaries[4];// = { 0, 0, 0, 0 };
     std::vector<Pedestrian*> _agents;
     std::shared_ptr<StartDistribution> _startDistribution;

};

#endif /* AGENTSOURCE_H_ */
