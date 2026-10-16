class Material {
private:
    int id;           // Material identifier
    double E;         // Young's modulus (Pa)
    double G;         // Shear modulus (Pa)
    double density = 0.0;   // mass density (modal analysis), consistent with the force/length units

public:

    Material(int id, double E, double G);

    //Geters and setters para boas práticas de orientação a objeto
    int getId() const;
    double getE() const;
    double getG() const;

    double getDensity() const { return density; }
    void setDensity(double value) { density = value; }
    void setE(double E);
    void setG(double G);
};