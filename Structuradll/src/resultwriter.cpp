#include "ResultWriter.h"
#include "Node.h"
#include "Member.h"
#include <iostream>
#include <iomanip>

ResultWriter::ResultWriter(const Structure& structure) : structure(structure) {}

bool ResultWriter::saveToFile(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Erro: Não foi possível abrir o arquivo de saída " << filename << std::endl;
        return false;
    }

    std::cout << "\nSalvando resultados em " << filename << "..." << std::endl;

    writeHeader(file);
    writeNodalDisplacements(file);
    writeReactions(file);
    writeMemberForces(file);
    writeMemberAuxiliaryVectors(file);

    file.close();
    std::cout << "Resultados salvos com sucesso." << std::endl;
    return true;
}

void ResultWriter::writeHeader(std::ofstream& file) const {
    file << "#----------------------------------------------------" << std::endl;
    file << "# ARQUIVO DE SAÍDA DA ANÁLISE ESTRUTURAL" << std::endl;
    file << "# Unidades: N, m, rad" << std::endl;
    file << "#----------------------------------------------------" << std::endl;
}

void ResultWriter::writeNodalDisplacements(std::ofstream& file) const {
    file << "\n[NODAL_DISPLACEMENTS]" << std::endl;
    file << "# Node_ID      UX (m)       UY (m)       UZ (m)       RX (rad)     RY (rad)     RZ (rad)" << std::endl;

    for (const auto& node : structure.getNodes()) {
        file << std::left << std::setw(8) << node->getId();
        for (int i = 0; i < 6; ++i) {
            // adding a space before each number
            file << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << node->getDisplacement(i);
        }
        file << std::endl;
    }
}

void ResultWriter::writeReactions(std::ofstream& file) const {
    file << "\n[REACTIONS]" << std::endl;
    file << "# Node_ID      Fx (N)       Fy (N)       Fz (N)       Mx (Nm)      My (Nm)      Mz (Nm)" << std::endl;

    for (const auto& node : structure.getNodes()) {
        bool hasReactions = false;
        for (int i = 0; i < 6; ++i) {
            if (node->isRestrained(i)) {
                hasReactions = true;
                break;
            }
        }

        if (hasReactions) {
            file << std::left << std::setw(8) << node->getId();
            for (int i = 0; i < 6; ++i) {
                // Adicionado um espaço e bla bla bla
                if (node->isRestrained(i)) {
                    file << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << node->getReaction(i);
                }
                else {
                    file << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << 0.0;
                }
            }
            file << std::endl;
        }
    }
}

void ResultWriter::writeMemberForces(std::ofstream& file) const {
    file << "\n[MEMBER_FORCES_LOCAL]" << std::endl;
    file << "# Forças nos nós de cada barra, no sistema local da barra" << std::endl;
    file << "# Member_ID  Node_ID      N (N)        Vy (N)       Vz (N)       T (Nm)       My (Nm)      Mz (Nm)" << std::endl;

    for (const auto& member : structure.getMembers()) {
        const auto& forces = member->getInternalForces();

        // forces at I node
        file << std::left << std::setw(11) << member->getId()
            << std::left << std::setw(12) << member->getNodeI()->getId();

        file << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << forces[0]   // N
            << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << forces[1]   // Vy
            << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << forces[2]   // Vz
            << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << forces[3]   // T
            << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << forces[4]   // My
            << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << forces[5] << std::endl; // Mz

        // forces at J node
        file << std::left << std::setw(11) << "" // Deixa a primeira coluna em branco
            << std::left << std::setw(12) << member->getNodeJ()->getId();

        file << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << forces[6]   // N
            << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << forces[7]   // Vy
            << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << forces[8]   // Vz
            << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << forces[9]   // T
            << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << forces[10]  // My
            << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << forces[11] << std::endl; // Mz
    }
}

void ResultWriter::writeMemberAuxiliaryVectors(std::ofstream& file) const {
    file << "\n[MEMBER_AUXILIARY_VECTOR_K]" << std::endl;
    file << "# Vetor auxiliar K associado a cada barra" << std::endl;
    file << "# Member_ID      Kx (-)        Ky (-)        Kz (-)" << std::endl;

    for (const auto& member : structure.getMembers()) {
        const Eigen::Vector3d kVector = member->getAuxiliaryVectorK();

        file << std::left << std::setw(11) << member->getId();
        file << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << kVector.x();
        file << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << kVector.y();
        file << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << kVector.z();
        file << std::endl;
    }
}
