#pragma once

#include <memory>
#include <string>
#include <vector>

#include <Eigen/Dense>

class Member;

class Load {
public:
    virtual ~Load() = default;
};

class NodalLoad : public Load {
private:
    int nodeId;
    std::vector<double> forces;

public:
    NodalLoad(int nodeId, const std::vector<double>& forces);

    int getNodeId() const;
    double getForce(int component) const;
    const std::vector<double>& getForces() const;
};

// Carga aplicada em uma barra: a classe base converte as forcas de engastamento perfeito da parte flexivel (e as das zonas rigidas) em
// forcas nodais equivalentes nos eixos globais. As subclasses so informam as componentes locais.
class BarLoad : public Load {
public:
    virtual int getMemberId() const = 0;
    // Forcas de engastamento perfeito (como cargas equivalentes) da parte flexivel nos eixos locais e cargas equivalentes das zonas rigidas
    virtual void localComponents(const Member* member, Eigen::VectorXd& flexLocal, Eigen::VectorXd& rigidAtNodeEnds) const = 0;
    // 12 componentes; 14 quando a barra tem torcao com empenamento (as duas ultimas sao as cargas equivalentes dos parametros de empenamento)
    std::vector<double> getEquivalentNodalForces(const Member* member) const;
};

class DistributedLoad : public BarLoad {
public:
    enum class CoordinateSystem { Local, Global };

private:
    int memberId;
    Eigen::Vector3d intensity;
    CoordinateSystem system;

public:
    DistributedLoad(int memberId, const Eigen::Vector3d& intensity, CoordinateSystem system);

    int getMemberId() const override;
    const Eigen::Vector3d& getIntensity() const;
    CoordinateSystem getCoordinateSystem() const;

    // Local-frame load components: clamped-end forces of the flexible part and equivalent loads of the rigid end zones
    void localComponents(const Member* member, Eigen::VectorXd& flexLocal, Eigen::VectorXd& rigidAtNodeEnds) const override;

    static CoordinateSystem coordinateSystemFromString(const std::string& token);
    static std::string coordinateSystemToString(CoordinateSystem system);
};

// Uniformly distributed moment per unit length (N*m/m) applied along a member.
class DistributedMoment : public BarLoad {
public:
    using CoordinateSystem = DistributedLoad::CoordinateSystem;

private:
    int memberId;
    Eigen::Vector3d intensity;
    CoordinateSystem system;

public:
    DistributedMoment(int memberId, const Eigen::Vector3d& intensity, CoordinateSystem system);

    int getMemberId() const override;
    void localComponents(const Member* member, Eigen::VectorXd& flexLocal, Eigen::VectorXd& rigidAtNodeEnds) const override;
};

// Carga geral de barra com posicao: trapezoidal/parcial, pontual, momento pontual e deformacao imposta (temperatura, retracao).
// As posicoes sao distancias medidas sobre o eixo da barra a partir do no I (comprimento total, incluindo zonas rigidas).
class MemberAction : public BarLoad {
public:
    enum class Kind { Trapezoid = 0, PointForce = 1, PointMoment = 2, Strain = 3 };
    using CoordinateSystem = DistributedLoad::CoordinateSystem;

private:
    int memberId;
    Kind kind;
    CoordinateSystem system;
    double a, b;
    Eigen::Vector3d v1, v2;

public:
    // Trapezoid: v1/v2 = forca por comprimento (qx,qy,qz) em `a` e em `b`. PointForce: v1 = (Fx,Fy,Fz) em `a`. PointMoment: v1 = (Mx,My,Mz) em `a`.
    // Strain: v1 = (deformacao axial livre, d(eps)/dy, d(eps)/dz [1/m]) em toda a barra (sempre nos eixos locais).
    MemberAction(int memberId, Kind kind, CoordinateSystem system, double a, double b, const Eigen::Vector3d& v1, const Eigen::Vector3d& v2);

    int getMemberId() const override { return memberId; }
    Kind getKind() const { return kind; }
    void localComponents(const Member* member, Eigen::VectorXd& flexLocal, Eigen::VectorXd& rigidAtNodeEnds) const override;
};
