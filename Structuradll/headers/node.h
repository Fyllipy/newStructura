#include <vector>

class Node {
private:
    int id;                           // Node identifier
    double x, y, z;                   // Node coordinates (m)
    std::vector<bool> restraints;     // Boundary conditions (6 DOF)
    std::vector<double> displacements; // Calculated displacements (6 DOF)
    std::vector<double> reactions;    // Reaction forces and moments

public:
    Node(int id, double x, double y, double z, const std::vector<bool>& restraints);

    int getId() const;
    double getX() const;
    double getY() const;
    double getZ() const;
    const std::vector<bool>& getRestraints() const;

    bool isRestrained(int dof) const;

    void setDisplacement(int dof, double value);

    double getDisplacement(int dof) const;

    const std::vector<double>& getDisplacements() const;

    void setReaction(int dof, double value);

    double getReaction(int dof) const;

    const std::vector<double>& getReactions() const;

    void setCoordinates(double x, double y, double z);
};