#include "Structure.h"
#include "ResultWriter.h"
#include "Node.h"
#include "Member.h"
#include "Material.h"
#include "Section.h"
#include "Load.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <stdexcept>
#include <cmath>
#include <Eigen/SparseCholesky>

Structure::Structure() : totalDOF(0) {}

bool Structure::readInputFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open input file " << filename << std::endl;
        return false;
    }

    std::string line;
    std::string currentSection;

    while (std::getline(file, line)) {
        line = cleanLine(line);

        // Skip empty lines
        if (line.empty()) {
            continue;
        }

        // Check for section headers
        if (line[0] == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.length() - 2);
            continue;
        }

        // Parse data lines
        if (!parseLine(line, currentSection)) {
            std::cerr << "Error parsing line: " << line << std::endl;
            return false;
        }
    }

    file.close();

    // Initialize DOF system
    initializeDOF();

    std::cout << "Successfully read input file with:" << std::endl;
    std::cout << "  Nodes: " << nodes.size() << std::endl;
    std::cout << "  Members: " << members.size() << std::endl;
    std::cout << "  Materials: " << materials.size() << std::endl;
    std::cout << "  Sections: " << sections.size() << std::endl;
    std::cout << "  Total DOF: " << totalDOF << std::endl;

    return true;
}

bool Structure::parseLine(const std::string& line, const std::string& section) {
    auto tokens = splitString(line);
    if (tokens.empty()) {
        return true;
    }

    try {
        if (section == "MATERIALS") {
            if (tokens.size() != 3) return false;
            int id = std::stoi(tokens[0]);
            double E = std::stod(tokens[1]);
            double G = std::stod(tokens[2]);

            auto material = std::make_shared<Material>(id, E, G);
            materials.push_back(material);
            materialMap[id] = material;

        }
        else if (section == "SECTIONS") {
            if (tokens.size() != 5) return false;
            int id = std::stoi(tokens[0]);
            double A = std::stod(tokens[1]);
            double Iz = std::stod(tokens[2]);
            double Iy = std::stod(tokens[3]);
            double J = std::stod(tokens[4]);

            auto section = std::make_shared<Section>(id, A, Iz, Iy, J);
            sections.push_back(section);
            sectionMap[id] = section;

        }
        else if (section == "NODES") {
            if (tokens.size() != 10) return false;
            int id = std::stoi(tokens[0]);
            double x = std::stod(tokens[1]);
            double y = std::stod(tokens[2]);
            double z = std::stod(tokens[3]);

            std::vector<bool> restraints(6);
            for (int i = 0; i < 6; i++) {
                restraints[i] = (std::stoi(tokens[i + 4]) == 1);
            }

            auto node = std::make_shared<Node>(id, x, y, z, restraints);
            nodes.push_back(node);
            nodeMap[id] = node;

        }
        else if (section == "MEMBERS") {
            if (tokens.size() != 5) return false;
            int id = std::stoi(tokens[0]);
            int nodeI_id = std::stoi(tokens[1]);
            int nodeJ_id = std::stoi(tokens[2]);
            int material_id = std::stoi(tokens[3]);
            int section_id = std::stoi(tokens[4]);

            auto nodeI = getNode(nodeI_id);
            auto nodeJ = getNode(nodeJ_id);
            auto material = getMaterial(material_id);
            auto section = getSection(section_id);

            if (!nodeI || !nodeJ || !material || !section) {
                std::cerr << "Error: Referenced node, material, or section not found" << std::endl;
                return false;
            }

            auto member = std::make_shared<Member>(id, nodeI, nodeJ, material, section);
            members.push_back(member);
            memberMap[id] = member;

        }
        else if (section == "NODAL_LOADS") {
            if (tokens.size() != 7) return false;
            int nodeId = std::stoi(tokens[0]);
            std::vector<double> forces(6);
            for (int i = 0; i < 6; i++) {
                forces[i] = std::stod(tokens[i + 1]);
            }

            auto load = std::make_shared<NodalLoad>(nodeId, forces);
            loads.push_back(load);

        }
        else if (section == "MEMBER_LOADS") {
            if (tokens.size() == 3) {
                int memberId = std::stoi(tokens[0]);
                double qy = std::stod(tokens[1]);
                double qz = std::stod(tokens[2]);

                Eigen::Vector3d intensity(0.0, qy, qz);
                auto load = std::make_shared<DistributedLoad>(memberId, intensity, DistributedLoad::CoordinateSystem::Local);
                loads.push_back(load);
            }
            else if (tokens.size() == 5) {
                int memberId = std::stoi(tokens[0]);
                auto system = DistributedLoad::coordinateSystemFromString(tokens[1]);
                double qx = std::stod(tokens[2]);
                double qy = std::stod(tokens[3]);
                double qz = std::stod(tokens[4]);

                Eigen::Vector3d intensity(qx, qy, qz);
                auto load = std::make_shared<DistributedLoad>(memberId, intensity, system);
                loads.push_back(load);
            }
            else {
                return false;
            }
        }

    }
    catch (const std::exception& e) {
        std::cerr << "Error parsing tokens: " << e.what() << std::endl;
        return false;
    }

    return true;
}

std::string Structure::cleanLine(const std::string& line) const {
    std::string cleaned = line;

    // Remove comments
    size_t commentPos = cleaned.find('#');
    if (commentPos != std::string::npos) {
        cleaned = cleaned.substr(0, commentPos);
    }

    // Trim whitespace
    cleaned.erase(0, cleaned.find_first_not_of(" \t"));
    cleaned.erase(cleaned.find_last_not_of(" \t") + 1);

    return cleaned;
}

std::vector<std::string> Structure::splitString(const std::string& str) const {
    std::vector<std::string> tokens;
    std::istringstream iss(str);
    std::string token;

    while (iss >> token) {
        tokens.push_back(token);
    }

    return tokens;
}

void Structure::initializeDOF() {
    totalDOF = nodes.size() * 6;

    // Classify DOF as free or restrained
    freeDOF.clear();
    restrainedDOF.clear();

    for (const auto& node : nodes) {
        int nodeId = node->getId();
        for (int localDOF = 0; localDOF < 6; localDOF++) {
            int globalDOF = getGlobalDOF(nodeId, localDOF);

            if (node->isRestrained(localDOF)) {
                restrainedDOF.push_back(globalDOF);
            }
            else {
                freeDOF.push_back(globalDOF);
            }
        }
    }

    std::cout << "  Free DOF: " << freeDOF.size() << std::endl;
    std::cout << "  Restrained DOF: " << restrainedDOF.size() << std::endl;
}

int Structure::getGlobalDOF(int nodeId, int localDOF) const {
    // Find node index
    int nodeIndex = -1;
    for (size_t i = 0; i < nodes.size(); i++) {
        if (nodes[i]->getId() == nodeId) {
            nodeIndex = i;
            break;
        }
    }

    if (nodeIndex == -1) {
        throw std::runtime_error("Node not found: " + std::to_string(nodeId));
    }

    return nodeIndex * 6 + localDOF;
}

void Structure::assembleGlobalStiffnessMatrix() {
    std::vector<Eigen::Triplet<double>> triplets;
    triplets.reserve(static_cast<size_t>(members.size()) * 144);

    for (const auto& member : members) {
        Eigen::MatrixXd memberStiffness = member->getGlobalStiffnessMatrix();
        std::vector<int> dofIndices(12);

        int nodeI_id = member->getNodeI()->getId();
        int nodeJ_id = member->getNodeJ()->getId();

        for (int i = 0; i < 6; ++i) {
            dofIndices[i] = getGlobalDOF(nodeI_id, i);
            dofIndices[i + 6] = getGlobalDOF(nodeJ_id, i);
        }

        for (int i = 0; i < 12; ++i) {
            for (int j = 0; j < 12; ++j) {
                double value = memberStiffness(i, j);
                if (std::abs(value) > 1e-12) {
                    triplets.emplace_back(dofIndices[i], dofIndices[j], value);
                }
            }
        }
    }

    globalStiffnessMatrix.resize(totalDOF, totalDOF);
    globalStiffnessMatrix.setZero();
    globalStiffnessMatrix.setFromTriplets(triplets.begin(), triplets.end());
    globalStiffnessMatrix.makeCompressed();

    std::cout << "Global stiffness matrix assembled (" << totalDOF << "x" << totalDOF << ")" << std::endl;
}

void Structure::assembleGlobalLoadVector() {
    globalLoadVector = Eigen::VectorXd::Zero(totalDOF);

    for (const auto& load : loads) {
        // Handle nodal loads
        if (auto nodalLoad = std::dynamic_pointer_cast<NodalLoad>(load)) {
            int nodeId = nodalLoad->getNodeId();

            for (int localDOF = 0; localDOF < 6; localDOF++) {
                int globalDOF = getGlobalDOF(nodeId, localDOF);
                globalLoadVector(globalDOF) += nodalLoad->getForce(localDOF);
            }
        }

        // Handle distributed loads
        if (auto distLoad = std::dynamic_pointer_cast<DistributedLoad>(load)) {
            int memberId = distLoad->getMemberId();
            auto member = getMember(memberId);

            if (member) {
                auto equivalentForces = distLoad->getEquivalentNodalForces(member.get());
                std::vector<int> dofIndices(12);


                int nodeI_id = member->getNodeI()->getId();
                int nodeJ_id = member->getNodeJ()->getId();

                // DOF indices for node I (0-5)
                for (int i = 0; i < 6; i++) {
                    dofIndices[i] = getGlobalDOF(nodeI_id, i);
                }

                // DOF indices for node J (6-11)
                for (int i = 0; i < 6; i++) {
                    dofIndices[i + 6] = getGlobalDOF(nodeJ_id, i);
                }

                // Add equivalent forces to global load vector
                for (int i = 0; i < 12; i++) {
                    globalLoadVector(dofIndices[i]) += equivalentForces[i];
                }
            }
        }
    }

    std::cout << "Global load vector assembled" << std::endl;
}

void Structure::applyBoundaryConditions() {
    if (restrainedDOF.size() == static_cast<size_t>(totalDOF)) {
        throw std::runtime_error("No free degrees of freedom - structure is overconstrained");
    }

    std::vector<bool> isRestrained(static_cast<size_t>(totalDOF), false);
    for (int dof : restrainedDOF) {
        if (dof < 0 || dof >= totalDOF) {
            throw std::runtime_error("Invalid restrained DOF encountered while applying boundary conditions");
        }
        isRestrained[static_cast<size_t>(dof)] = true;
    }

    std::vector<Eigen::Triplet<double>> triplets;
    triplets.reserve(globalStiffnessMatrix.nonZeros());

    for (int col = 0; col < globalStiffnessMatrix.outerSize(); ++col) {
        for (Eigen::SparseMatrix<double>::InnerIterator it(globalStiffnessMatrix, col); it; ++it) {
            int row = it.row();
            if (!isRestrained[static_cast<size_t>(row)] && !isRestrained[static_cast<size_t>(col)]) {
                triplets.emplace_back(row, col, it.value());
            }
        }
    }

    for (int dof : restrainedDOF) {
        triplets.emplace_back(dof, dof, 1.0);
        globalLoadVector(dof) = 0.0; // Zero displacement prescribed for restrained DOF
    }

    Eigen::SparseMatrix<double> modified(totalDOF, totalDOF);
    modified.setFromTriplets(triplets.begin(), triplets.end());
    modified.makeCompressed();

    globalStiffnessMatrix = modified;

    std::cout << "Boundary conditions applied using zeros-and-ones technique." << std::endl;
}

bool Structure::solve() {
    try {
        // Check if system is solvable
        if (globalStiffnessMatrix.rows() == 0) {
            std::cerr << "Error: Empty stiffness matrix" << std::endl;
            return false;
        }

        Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver;
        solver.compute(globalStiffnessMatrix);
        if (solver.info() != Eigen::Success) {
            std::cerr << "Error: Failed to decompose stiffness matrix" << std::endl;
            return false;
        }

        globalDisplacementVector = solver.solve(globalLoadVector);
        if (solver.info() != Eigen::Success) {
            std::cerr << "Error: Failed to solve linear system" << std::endl;
            return false;
        }

        double residual = (globalStiffnessMatrix * globalDisplacementVector - globalLoadVector).norm();
        if (residual > 1e-6) {
            std::cerr << "Warning: Large residual in solution: " << residual << std::endl;
        }

        // Distribute displacements back to nodes
        for (const auto& node : nodes) {
            int nodeId = node->getId();
            for (int localDOF = 0; localDOF < 6; ++localDOF) {
                int globalDOF = getGlobalDOF(nodeId, localDOF);
                if (globalDOF < globalDisplacementVector.size()) {
                    node->setDisplacement(localDOF, globalDisplacementVector(globalDOF));
                }
            }
        }

        std::cout << "System solved successfully" << std::endl;
        return true;

    }
    catch (const std::exception& e) {
        std::cerr << "Error solving system: " << e.what() << std::endl;
        return false;
    }
}

void Structure::calculateReactions() {
    // Reassemble full stiffness matrix and load vector
    assembleGlobalStiffnessMatrix();
    assembleGlobalLoadVector();

    // Create full displacement vector
    Eigen::VectorXd fullDisplacementVector = Eigen::VectorXd::Zero(totalDOF);

    // Fill in known displacements
    for (const auto& node : nodes) {
        int nodeId = node->getId();
        for (int localDOF = 0; localDOF < 6; localDOF++) {
            int globalDOF = getGlobalDOF(nodeId, localDOF);
            fullDisplacementVector(globalDOF) = node->getDisplacement(localDOF);
        }
    }

    // Calculate reaction forces: {R} = [K]{U} - {F}
    Eigen::VectorXd reactionVector = globalStiffnessMatrix * fullDisplacementVector - globalLoadVector;

    // Store reactions in nodes
    for (const auto& node : nodes) {
        int nodeId = node->getId();
        for (int localDOF = 0; localDOF < 6; localDOF++) {
            if (node->isRestrained(localDOF)) {
                int globalDOF = getGlobalDOF(nodeId, localDOF);
                node->setReaction(localDOF, reactionVector(globalDOF));
            }
        }
    }

    std::cout << "Reactions calculated" << std::endl;
}

void Structure::postProcess() {
    for (auto& member : members) {
        member->clearDistributedLoads();
    }

    for (const auto& load : loads) {
        // Handle distributed loads
        if (auto distLoad = std::dynamic_pointer_cast<DistributedLoad>(load)) {
            int memberId = distLoad->getMemberId();
            auto member = getMember(memberId);

            if (member) {
                auto equivalentForces = distLoad->getEquivalentNodalForces(member.get());
                member->addDistributedLoad(equivalentForces);

                const auto& intensity = distLoad->getIntensity();
                auto systemLabel = DistributedLoad::coordinateSystemToString(distLoad->getCoordinateSystem());
                std::cout << "Applied distributed load to member " << memberId
                    << " (" << systemLabel << ")"
                    << ": qx=" << intensity.x()
                    << ", qy=" << intensity.y()
                    << ", qz=" << intensity.z() << std::endl;
            }
        }
    }

    // Calculate internal forces for all members
    for (auto& member : members) {
        try {
            member->calculateInternalForces();
        }
        catch (const std::exception& e) {
            std::cerr << "Error calculating internal forces for member "
                << member->getId() << ": " << e.what() << std::endl;
        }
    }

    std::cout << "Internal forces calculated with distributed loads correction" << std::endl;
}

void Structure::printResults() const {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "                    STRUCTURAL ANALYSIS RESULTS" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    // Print nodal displacements
    std::cout << "\nNODAL DISPLACEMENTS:" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    std::cout << std::setw(6) << "Node"
        << std::setw(12) << "UX (m)"
        << std::setw(12) << "UY (m)"
        << std::setw(12) << "UZ (m)"
        << std::setw(12) << "RX (rad)"
        << std::setw(12) << "RY (rad)"
        << std::setw(12) << "RZ (rad)" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    for (const auto& node : nodes) {
        std::cout << std::setw(6) << node->getId();
        for (int i = 0; i < 6; i++) {
            std::cout << std::setw(12) << std::scientific << std::setprecision(4)
                << node->getDisplacement(i);
        }
        std::cout << std::endl;
    }

    // Print reactions
    std::cout << "\nREACTION FORCES:" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    std::cout << std::setw(6) << "Node"
        << std::setw(12) << "FX (N)"
        << std::setw(12) << "FY (N)"
        << std::setw(12) << "FZ (N)"
        << std::setw(12) << "MX (Nm)"
        << std::setw(12) << "MY (Nm)"
        << std::setw(12) << "MZ (Nm)" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    for (const auto& node : nodes) {
        bool hasReactions = false;
        for (int i = 0; i < 6; i++) {
            if (node->isRestrained(i)) {
                hasReactions = true;
                break;
            }
        }

        if (hasReactions) {
            std::cout << std::setw(6) << node->getId();
            for (int i = 0; i < 6; i++) {
                if (node->isRestrained(i)) {
                    std::cout << std::setw(12) << std::scientific << std::setprecision(4)
                        << node->getReaction(i);
                }
                else {
                    std::cout << std::setw(12) << "-";
                }
            }
            std::cout << std::endl;
        }
    }

    // Print member internal forces
    std::cout << "\nMEMBER INTERNAL FORCES:" << std::endl;
    std::cout << std::string(120, '-') << std::endl;
    std::cout << std::setw(6) << "Member"
        << std::setw(8) << "End"
        << std::setw(12) << "Fx (N)"
        << std::setw(12) << "Fy (N)"
        << std::setw(12) << "Fz (N)"
        << std::setw(12) << "Mx (Nm)"
        << std::setw(12) << "My (Nm)"
        << std::setw(12) << "Mz (Nm)" << std::endl;
    std::cout << std::string(120, '-') << std::endl;

    for (const auto& member : members) {
        const auto& forces = member->getInternalForces();

        // Forces at node I
        std::cout << std::setw(6) << member->getId()
            << std::setw(8) << "I";
        for (int i = 0; i < 6; i++) {
            std::cout << std::setw(12) << std::scientific << std::setprecision(4)
                << forces[i];
        }
        std::cout << std::endl;

        // Forces at node J
        std::cout << std::setw(6) << ""
            << std::setw(8) << "J";
        for (int i = 6; i < 12; i++) {
            std::cout << std::setw(12) << std::scientific << std::setprecision(4)
                << forces[i];
        }
        std::cout << std::endl;
    }

    std::cout << "\nAUXILIARY VECTOR K PER MEMBER:" << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    std::cout << std::setw(6) << "Member"
        << std::setw(16) << "Kx"
        << std::setw(16) << "Ky"
        << std::setw(16) << "Kz" << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    for (const auto& member : members) {
        const Eigen::Vector3d kVector = member->getAuxiliaryVectorK();
        std::cout << std::setw(6) << member->getId()
            << std::setw(16) << std::scientific << std::setprecision(4) << kVector.x()
            << std::setw(16) << std::scientific << std::setprecision(4) << kVector.y()
            << std::setw(16) << std::scientific << std::setprecision(4) << kVector.z()
            << std::endl;
    }

    std::cout << std::string(120, '=') << std::endl;
}

std::shared_ptr<Node> Structure::getNode(int id) const {
    auto it = nodeMap.find(id);
    return (it != nodeMap.end()) ? it->second : nullptr;
}

std::shared_ptr<Member> Structure::getMember(int id) const {
    auto it = memberMap.find(id);
    return (it != memberMap.end()) ? it->second : nullptr;
}

std::shared_ptr<Material> Structure::getMaterial(int id) const {
    auto it = materialMap.find(id);
    return (it != materialMap.end()) ? it->second : nullptr;
}

std::shared_ptr<Section> Structure::getSection(int id) const {
    auto it = sectionMap.find(id);
    return (it != sectionMap.end()) ? it->second : nullptr;
}

int Structure::getTotalDOF() const {
    return totalDOF;
}

void Structure::saveResultsToFile(const std::string& filename) const {
    ResultWriter writer(*this);
    if (!writer.saveToFile(filename)) {
        std::cerr << "Falha ao salvar os resultados no arquivo." << std::endl; //Todo: Colocar isso em inglÃªs depois para boas prÃ¡ticas!!!
    }
}

const std::vector<std::shared_ptr<Node>>& Structure::getNodes() const {
    return nodes;
}

const std::vector<std::shared_ptr<Member>>& Structure::getMembers() const {
    return members;
}

