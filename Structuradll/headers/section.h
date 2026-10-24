class Section {
private:
    int id;           // Section identifier
    double A;         // Cross-sectional area (m²)
    double Iz;        // Moment of inertia about z-axis (m⁴)
    double Iy;        // Moment of inertia about y-axis (m⁴)
    double J;         // Torsional constant (m⁴)
    double Ay = 0.0;  // Shear area for shear along local y (m²); 0 = rigid in shear
    double Az = 0.0;  // Shear area for shear along local z (m²); 0 = rigid in shear
    double Cw = 0.0;  // Warping constant (m⁶); 0 = no warping torsion

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

    double getAy() const;
    double getAz() const;
    void setShearAreas(double Ay, double Az);

    double getCw() const;
    void setCw(double Cw);
};