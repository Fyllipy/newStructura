#include "section.h"

Section::Section(int id, double A, double Iz, double Iy, double J)
    : id(id), A(A), Iz(Iz), Iy(Iy), J(J) {
}

int Section::getId() const {
    return id;
}

double Section::getA() const {
    return A;
}

double Section::getIz() const {
    return Iz;
}

double Section::getIy() const {
    return Iy;
}

double Section::getJ() const {
    return J;
}

void Section::setA(double A) {
    this->A = A;
}

void Section::setIz(double Iz) {
    this->Iz = Iz;
}

void Section::setIy(double Iy) {
    this->Iy = Iy;
}

void Section::setJ(double J) {
    this->J = J;
}

double Section::getAy() const {
    return Ay;
}

double Section::getAz() const {
    return Az;
}

void Section::setShearAreas(double Ay, double Az) {
    this->Ay = Ay;
    this->Az = Az;
}

double Section::getCw() const {
    return Cw;
}

void Section::setCw(double Cw) {
    this->Cw = Cw;
}
