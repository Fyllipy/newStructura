#include "output_writer.h"

#include <fstream>
#include <iomanip>
#include <unordered_map>

namespace {
void writeHeader(std::ofstream& file) {
    file << "#----------------------------------------------------\n";
    file << "# ARQUIVO DE SAIDA DA ANALISE ESTRUTURAL\n";
    file << "# Unidades: N, m, rad\n";
    file << "#----------------------------------------------------\n";
}

void writeNodalDisplacements(std::ofstream& file, const SolverOutput& output) {
    file << "\n[NODAL_DISPLACEMENTS]\n";
    file << "# Node_ID      UX (m)       UY (m)       UZ (m)       RX (rad)     RY (rad)     RZ (rad)\n";

    for (const auto& node : output.nodes) {
        file << std::left << std::setw(8) << node.id;
        for (double value : node.displacements) {
            file << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << value;
        }
        file << '\n';
    }
}

void writeReactions(
    std::ofstream& file,
    const SolverInput& input,
    const SolverOutput& output) {
    std::unordered_map<int, std::array<int, 6>> restraintsByNode;
    restraintsByNode.reserve(input.nodes.size());
    for (const auto& node : input.nodes) {
        restraintsByNode[node.id] = node.restraints;
    }

    file << "\n[REACTIONS]\n";
    file << "# Node_ID      Fx (N)       Fy (N)       Fz (N)       Mx (Nm)      My (Nm)      Mz (Nm)\n";

    for (const auto& node : output.nodes) {
        const auto it = restraintsByNode.find(node.id);
        if (it == restraintsByNode.end()) {
            continue;
        }

        bool hasRestraint = false;
        for (int restraint : it->second) {
            if (restraint != 0) {
                hasRestraint = true;
                break;
            }
        }

        if (!hasRestraint) {
            continue;
        }

        file << std::left << std::setw(8) << node.id;
        for (size_t i = 0; i < node.reactions.size(); ++i) {
            const double value = (it->second[i] != 0) ? node.reactions[i] : 0.0;
            file << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << value;
        }
        file << '\n';
    }
}

void writeMemberForces(std::ofstream& file, const SolverOutput& output) {
    file << "\n[MEMBER_FORCES_LOCAL]\n";
    file << "# Forcas nos nos de cada barra, no sistema local da barra\n";
    file << "# Member_ID  Node_ID      N (N)        Vy (N)       Vz (N)       T (Nm)       My (Nm)      Mz (Nm)\n";

    for (const auto& member : output.members) {
        file << std::left << std::setw(11) << member.id
             << std::left << std::setw(12) << member.nodeI;

        for (int i = 0; i < 6; ++i) {
            file << " " << std::right << std::scientific << std::setprecision(6)
                 << std::setw(13) << member.localForces[static_cast<size_t>(i)];
        }
        file << '\n';

        file << std::left << std::setw(11) << ""
             << std::left << std::setw(12) << member.nodeJ;

        for (int i = 6; i < 12; ++i) {
            file << " " << std::right << std::scientific << std::setprecision(6)
                 << std::setw(13) << member.localForces[static_cast<size_t>(i)];
        }
        file << '\n';
    }
}

void writeMemberAuxiliaryVectors(std::ofstream& file, const SolverOutput& output) {
    file << "\n[MEMBER_AUXILIARY_VECTOR_K]\n";
    file << "# Vetor auxiliar K associado a cada barra\n";
    file << "# Member_ID      Kx (-)        Ky (-)        Kz (-)\n";

    for (const auto& member : output.members) {
        file << std::left << std::setw(11) << member.id;
        file << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << member.auxiliaryK[0];
        file << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << member.auxiliaryK[1];
        file << " " << std::right << std::scientific << std::setprecision(6) << std::setw(13) << member.auxiliaryK[2];
        file << '\n';
    }
}
} // namespace

bool writeOutputFile(
    const std::string& filePath,
    const SolverInput& input,
    const SolverOutput& output,
    std::string& errorMessage) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        errorMessage = "Could not open output file: " + filePath;
        return false;
    }

    writeHeader(file);
    writeNodalDisplacements(file, output);
    writeReactions(file, input, output);
    writeMemberForces(file, output);
    writeMemberAuxiliaryVectors(file, output);

    errorMessage.clear();
    return true;
}
