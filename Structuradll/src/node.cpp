#include "node.h"
#include <stdexcept>

Node::Node(int id, double x, double y, double z, const std::vector<bool>& restraints)
    : id(id), x(x), y(y), z(z), restraints(restraints) {

    // Initialize displacement and reaction vectors with zeros
    displacements.resize(6, 0.0);
    reactions.resize(6, 0.0);

    // Ensure restraints vector has exactly 6 elements
    if (this->restraints.size() != 6) {
        throw std::invalid_argument("Restraints vector must have exactly 6 elements");
    }
}

int Node::getId() const {
    return id;
}

double Node::getX() const {
    return x;
}

double Node::getY() const {
    return y;
}

double Node::getZ() const {
    return z;
}

const std::vector<bool>& Node::getRestraints() const {
    return restraints;
}

bool Node::isRestrained(int dof) const {
    if (dof < 0 || dof >= 6) {
        throw std::out_of_range("DOF index must be between 0 and 5");
    }
    return restraints[dof];
}

void Node::setDisplacement(int dof, double value) {
    if (dof < 0 || dof >= 6) {
        throw std::out_of_range("DOF index must be between 0 and 5");
    }
    displacements[dof] = value;
}

double Node::getDisplacement(int dof) const {
    if (dof < 0 || dof >= 6) {
        throw std::out_of_range("DOF index must be between 0 and 5");
    }
    return displacements[dof];
}

const std::vector<double>& Node::getDisplacements() const {
    return displacements;
}

void Node::setReaction(int dof, double value) {
    if (dof < 0 || dof >= 6) {
        throw std::out_of_range("DOF index must be between 0 and 5");
    }
    reactions[dof] = value;
}

double Node::getReaction(int dof) const {
    if (dof < 0 || dof >= 6) {
        throw std::out_of_range("DOF index must be between 0 and 5");
    }
    return reactions[dof];
}

const std::vector<double>& Node::getReactions() const {
    return reactions;
}

void Node::setCoordinates(double x, double y, double z) {
    this->x = x;
    this->y = y;
    this->z = z;
}