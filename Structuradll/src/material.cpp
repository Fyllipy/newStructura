#include "material.h"

Material::Material(int id, double E, double G) : id(id), E(E), G(G) {}

int Material::getId() const {
    return id;
}

double Material::getE() const {
    return E;
}

double Material::getG() const {
    return G;
}

void Material::setE(double E) {
    this->E = E;
}

void Material::setG(double G) {
    this->G = G;
}