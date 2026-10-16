#include <string>
#include <fstream>
#include <vector>
#include <memory>
#include "Structure.h" // Access structure data

class ResultWriter {
public:
    explicit ResultWriter(const Structure& structure);

    bool saveToFile(const std::string& filename) const;

private:
    const Structure& structure; // Reference to the solved structure

    void writeHeader(std::ofstream& file) const;
    void writeNodalDisplacements(std::ofstream& file) const;
    void writeReactions(std::ofstream& file) const;
    void writeMemberForces(std::ofstream& file) const;
    void writeMemberAuxiliaryVectors(std::ofstream& file) const;
};
