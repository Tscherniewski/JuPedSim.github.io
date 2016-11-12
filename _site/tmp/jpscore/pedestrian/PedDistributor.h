/**
 * \file        PedDistributor.h
 * \date        Oct 12, 2010
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

#ifndef _PEDDISTRIBUTOR_H
#define _PEDDISTRIBUTOR_H

#include <vector>
#include <string>
#include <memory>

#include "../routing/Router.h"
#include "../geometry/Building.h"
#include "AgentsParameters.h"
#include "AgentsSource.h"
#include "StartDistribution.h"
#include "../general/ArgumentParser.h"

//typedef vector<Point> tPoints;
//typedef vector<tPoints> GridPoints;


class PedDistributor {
private:
    std::vector<std::shared_ptr<StartDistribution> > _start_dis; // ID startraum, subroom und Anz
    std::vector<std::shared_ptr<StartDistribution> > _start_dis_sub; // ID startraum, subroom und Anz
    std::vector<std::shared_ptr<AgentsSource> > _start_dis_sources; // contain the sources
    //std::string _projectFilename; // store the file for later user
    //std::map<int, AgentsParameters*> _agentsParameters;

    static std::vector<Point> PositionsOnFixX(double max_x, double min_x, double max_y,
                                              double min_y, const SubRoom &r, double bufx, double bufy, double dy);

    static std::vector<Point> PositionsOnFixY(double max_x, double min_x, double max_y,
                                              double min_y, const SubRoom &r, double bufx, double bufy, double dx);

    const Configuration * _configuration;
public:
    /**
     * constructor
     */
    PedDistributor(const Configuration *configuration);

    /**
     * desctructor
     */
    virtual ~PedDistributor();

    /**
     * Return the possible positions for distributing the agents in the subroom
     */
    static vector<Point> PossiblePositions(const SubRoom &r);

    /**
     * Distribute the pedestrians in the Subroom with the given parameters
     */
    void DistributeInSubRoom(int N, std::vector<Point> &positions, int *pid,
                             StartDistribution *parameters, Building *building) const;

    /**
     *
     *Distribute all agents based on the configuration (ini) file
     * @return true if everything went fine
     */
    bool Distribute(Building *building) const;

    /**
     * provided for convenience
     */

    const std::vector<std::shared_ptr<AgentsSource> > &GetAgentsSources() const;

};

#endif  /* _PEDDISTRIBUTOR_H */
