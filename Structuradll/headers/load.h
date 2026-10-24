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

class DistributedLoad : public Load {
public:
    enum class CoordinateSystem { Local, Global };

private:
    int memberId;
    Eigen::Vector3d intensity;
    CoordinateSystem system;

public:
    DistributedLoad(int memberId, const Eigen::Vector3d& intensity, CoordinateSystem system);

    int getMemberId() const;
    const Eigen::Vector3d& getIntensity() const;
    CoordinateSystem getCoordinateSystem() const;

    std::vector<double> getEquivalentNodalForces(const Member* member) const;
    // Local-frame load components: clamped-end forces of the flexible part and equivalent loads of the rigid end zones
    void localComponents(const Member* member, Eigen::VectorXd& flexLocal, Eigen::VectorXd& rigidAtNodeEnds) const;

    static CoordinateSystem coordinateSystemFromString(const std::string& token);
    static std::string coordinateSystemToString(CoordinateSystem system);
};

// Uniformly distributed moment per unit length (N*m/m) applied along a member.
class DistributedMoment : public Load {
public:
    using CoordinateSystem = DistributedLoad::CoordinateSystem;

private:
    int memberId;
    Eigen::Vector3d intensity;
    CoordinateSystem system;

public:
    DistributedMoment(int memberId, const Eigen::Vector3d& intensity, CoordinateSystem system);

    int getMemberId() const;
    std::vector<double> getEquivalentNodalForces(const Member* member) const;
    void localComponents(const Member* member, Eigen::VectorXd& flexLocal, Eigen::VectorXd& rigidAtNodeEnds) const;
};
