#ifndef STRUCTURE_H
#define STRUCTURE_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <Eigen/Dense>
#include <Eigen/Sparse>

// Forward declarations
class Node;
class Member;
class Material;
class Section;
class Load;
class NodalLoad;
class DistributedLoad;
class ResultWriter;

class Structure {
private:
    std::vector<std::shared_ptr<Node>> nodes;
    std::vector<std::shared_ptr<Member>> members;
    std::vector<std::shared_ptr<Material>> materials;
    std::vector<std::shared_ptr<Section>> sections;
    std::vector<std::shared_ptr<Load>> loads;

    std::map<int, std::shared_ptr<Node>> nodeMap;
    std::map<int, std::shared_ptr<Member>> memberMap;
    std::map<int, std::shared_ptr<Material>> materialMap;
    std::map<int, std::shared_ptr<Section>> sectionMap;

    Eigen::SparseMatrix<double> globalStiffnessMatrix;
    Eigen::VectorXd globalLoadVector;
    Eigen::VectorXd globalDisplacementVector;

    int totalDOF;
    std::vector<int> freeDOF;
    std::vector<int> restrainedDOF;

public:
    Structure();
    ~Structure() = default;

    bool readInputFile(const std::string& filename);

    void assembleGlobalStiffnessMatrix();
    void assembleGlobalLoadVector();
    void applyBoundaryConditions();
    bool solve();
    void calculateReactions();
    void postProcess();
    void printResults() const;
    void saveResultsToFile(const std::string& filename) const;

    const std::vector<std::shared_ptr<Node>>& getNodes() const;
    const std::vector<std::shared_ptr<Member>>& getMembers() const;
    std::shared_ptr<Node> getNode(int id) const;
    std::shared_ptr<Member> getMember(int id) const;
    std::shared_ptr<Material> getMaterial(int id) const;
    std::shared_ptr<Section> getSection(int id) const;
    int getTotalDOF() const;

private:
    void initializeDOF();
    bool parseLine(const std::string& line, const std::string& section);
    std::string cleanLine(const std::string& line) const;
    std::vector<std::string> splitString(const std::string& str) const;
    int getGlobalDOF(int nodeId, int localDOF) const;
};

#endif // STRUCTURE_H
