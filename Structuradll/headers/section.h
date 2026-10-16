class Section {
private:
    int id;           // Section identifier
    double A;         // Cross-sectional area (m²)
    double Iz;        // Moment of inertia about z-axis (m⁴)
    double Iy;        // Moment of inertia about y-axis (m⁴)
    double J;         // Torsional constant (m⁴)

public:
    Section(int id, double A, double Iz, double Iy, double J);

    int getId() const;
    double getA() const;
    double getIz() const;
    double getIy() const;
    double getJ() const;

    void setA(double A);
    void setIz(double Iz);
    void setIy(double Iy);
    void setJ(double J);
};