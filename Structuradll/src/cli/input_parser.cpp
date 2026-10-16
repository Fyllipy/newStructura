#include "input_parser.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
std::string trim(const std::string& text) {
    const size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return std::string();
    }

    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::string cleanLine(const std::string& line) {
    std::string cleaned = line;
    if (cleaned.size() >= 3 &&
        static_cast<unsigned char>(cleaned[0]) == 0xEF &&
        static_cast<unsigned char>(cleaned[1]) == 0xBB &&
        static_cast<unsigned char>(cleaned[2]) == 0xBF) {
        cleaned = cleaned.substr(3);
    }

    const size_t commentPos = cleaned.find('#');
    if (commentPos != std::string::npos) {
        cleaned = cleaned.substr(0, commentPos);
    }
    return trim(cleaned);
}

std::vector<std::string> splitTokens(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream stream(line);
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

DistributedLoadSystem parseCoordinateSystem(const std::string& token) {
    std::string normalized = token;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });

    if (normalized == "LOCAL" || normalized == "L") {
        return DistributedLoadSystem::Local;
    }
    if (normalized == "GLOBAL" || normalized == "G") {
        return DistributedLoadSystem::Global;
    }

    throw std::runtime_error("Unknown coordinate system token: " + token);
}

void parseMaterial(const std::vector<std::string>& tokens, SolverInput& input) {
    if (tokens.size() < 3) {
        throw std::runtime_error("[MATERIALS] requires 3 columns");
    }

    MaterialData material;
    material.id = std::stoi(tokens[0]);
    material.E = std::stod(tokens[1]);
    material.G = std::stod(tokens[2]);
    input.materials.push_back(material);
}

void parseSection(const std::vector<std::string>& tokens, SolverInput& input) {
    if (tokens.size() < 5) {
        throw std::runtime_error("[SECTIONS] requires 5 columns");
    }

    SectionData section;
    section.id = std::stoi(tokens[0]);
    section.A = std::stod(tokens[1]);
    section.Iz = std::stod(tokens[2]);
    section.Iy = std::stod(tokens[3]);
    section.J = std::stod(tokens[4]);
    input.sections.push_back(section);
}

void parseNode(const std::vector<std::string>& tokens, SolverInput& input) {
    if (tokens.size() < 10) {
        throw std::runtime_error("[NODES] requires 10 columns");
    }

    NodeData node;
    node.id = std::stoi(tokens[0]);
    node.x = std::stod(tokens[1]);
    node.y = std::stod(tokens[2]);
    node.z = std::stod(tokens[3]);
    for (int i = 0; i < 6; ++i) {
        node.restraints[static_cast<size_t>(i)] = std::stoi(tokens[static_cast<size_t>(i + 4)]);
    }

    input.nodes.push_back(node);
}

void parseMember(const std::vector<std::string>& tokens, SolverInput& input) {
    if (tokens.size() < 5) {
        throw std::runtime_error("[MEMBERS] requires 5 columns");
    }

    MemberData member;
    member.id = std::stoi(tokens[0]);
    member.nodeI = std::stoi(tokens[1]);
    member.nodeJ = std::stoi(tokens[2]);
    member.materialId = std::stoi(tokens[3]);
    member.sectionId = std::stoi(tokens[4]);
    // Optional: rotation(deg) [dy1 dz1 dy2 dz2 (m)]
    if (tokens.size() >= 6) {
        member.sectionRotationDeg = std::stod(tokens[5]);
    }
    if (tokens.size() >= 10) {
        member.offsetY1 = std::stod(tokens[6]);
        member.offsetZ1 = std::stod(tokens[7]);
        member.offsetY2 = std::stod(tokens[8]);
        member.offsetZ2 = std::stod(tokens[9]);
    }
    input.members.push_back(member);
}

void parseNodalLoad(const std::vector<std::string>& tokens, SolverInput& input) {
    if (tokens.size() < 7) {
        throw std::runtime_error("[NODAL_LOADS] requires 7 columns");
    }

    NodalLoadData load;
    load.nodeId = std::stoi(tokens[0]);
    for (int i = 0; i < 6; ++i) {
        load.values[static_cast<size_t>(i)] = std::stod(tokens[static_cast<size_t>(i + 1)]);
    }

    input.nodalLoads.push_back(load);
}

void parseMemberLoad(const std::vector<std::string>& tokens, SolverInput& input) {
    MemberLoadData load;

    if (tokens.size() == 3) {
        load.memberId = std::stoi(tokens[0]);
        load.system = DistributedLoadSystem::Local;
        load.qx = 0.0;
        load.qy = std::stod(tokens[1]);
        load.qz = std::stod(tokens[2]);
        input.memberLoads.push_back(load);
        return;
    }

    if (tokens.size() >= 5) {
        load.memberId = std::stoi(tokens[0]);
        load.system = parseCoordinateSystem(tokens[1]);
        load.qx = std::stod(tokens[2]);
        load.qy = std::stod(tokens[3]);
        load.qz = std::stod(tokens[4]);
        input.memberLoads.push_back(load);
        return;
    }

    throw std::runtime_error("[MEMBER_LOADS] requires 3 or 5 columns");
}

void parseMemberMoment(const std::vector<std::string>& tokens, SolverInput& input) {
    if (tokens.size() < 5) {
        throw std::runtime_error("[MEMBER_MOMENTS] requires at least 5 columns");
    }

    MemberMomentData moment;
    moment.memberId = std::stoi(tokens[0]);
    moment.system = parseCoordinateSystem(tokens[1]);
    moment.mx = std::stod(tokens[2]);
    moment.my = std::stod(tokens[3]);
    moment.mz = std::stod(tokens[4]);
    input.memberMoments.push_back(moment);
}

void parseSectionLine(const std::string& section, const std::vector<std::string>& tokens, SolverInput& input) {
    if (section == "MATERIALS") {
        parseMaterial(tokens, input);
        return;
    }
    if (section == "SECTIONS") {
        parseSection(tokens, input);
        return;
    }
    if (section == "NODES") {
        parseNode(tokens, input);
        return;
    }
    if (section == "MEMBERS") {
        parseMember(tokens, input);
        return;
    }
    if (section == "NODAL_LOADS") {
        parseNodalLoad(tokens, input);
        return;
    }
    if (section == "MEMBER_LOADS") {
        parseMemberLoad(tokens, input);
        return;
    }
    if (section == "MEMBER_MOMENTS") {
        parseMemberMoment(tokens, input);
        return;
    }
}
} // namespace

bool parseInputFile(const std::string& filePath, SolverInput& input, std::string& errorMessage) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open input file: " + filePath);
        }

        input = SolverInput{};
        std::string section;
        std::string line;
        int lineNumber = 0;

        while (std::getline(file, line)) {
            ++lineNumber;
            const std::string cleanedLine = cleanLine(line);
            if (cleanedLine.empty()) {
                continue;
            }

            if (cleanedLine.front() == '[' && cleanedLine.back() == ']') {
                section = cleanedLine.substr(1, cleanedLine.size() - 2);
                continue;
            }

            if (section.empty()) {
                throw std::runtime_error("Data line found before any section header");
            }

            const auto tokens = splitTokens(cleanedLine);
            parseSectionLine(section, tokens, input);
        }

        errorMessage.clear();
        return true;
    }
    catch (const std::exception& ex) {
        errorMessage = ex.what();
        return false;
    }
}
