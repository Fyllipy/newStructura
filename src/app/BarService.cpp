#include "BarService.h"
#include <algorithm>

namespace Structura::App {

BarService::BarService(IModelRepository *repository, QObject *parent)
    : QObject(parent)
    , m_repository(repository)
{
    Q_ASSERT(repository != nullptr);
}

QUuid BarService::createBar(const QUuid &startNodeId,
                            const QUuid &endNodeId,
                            const QUuid &materialId,
                            const QUuid &sectionId)
{
    // Validate that nodes exist
    if (!m_repository->findNode(startNodeId).has_value() ||
        !m_repository->findNode(endNodeId).has_value()) {
        return QUuid(); // Invalid nodes
    }
    
    // Ensure start and end nodes are different
    if (startNodeId == endNodeId) {
        return QUuid(); // Cannot connect node to itself
    }
    
    int externalId = nextExternalId();
    QUuid id = QUuid::createUuid();
    
    Bar bar(id, startNodeId, endNodeId, materialId, sectionId);
    bar.setExternalId(externalId);
    
    if (m_repository->addBar(bar)) {
        emit barCreated(id);
        return id;
    }
    
    return QUuid();
}

bool BarService::deleteBar(const QUuid &id)
{
    if (m_repository->removeBar(id)) {
        emit barDeleted(id);
        return true;
    }
    return false;
}

bool BarService::updateBarConnectivity(const QUuid &id, const QUuid &startNodeId, const QUuid &endNodeId)
{
    // Validate that nodes exist
    if (!m_repository->findNode(startNodeId).has_value() ||
        !m_repository->findNode(endNodeId).has_value()) {
        return false;
    }
    
    // Ensure start and end nodes are different
    if (startNodeId == endNodeId) {
        return false;
    }
    
    auto barOpt = m_repository->findBar(id);
    if (!barOpt.has_value()) {
        return false;
    }
    
    Bar bar = barOpt.value();
    bar.setStartNodeId(startNodeId);
    bar.setEndNodeId(endNodeId);
    bar.setLCSDirty(true);
    
    if (m_repository->updateBar(bar)) {
        emit barUpdated(id);
        return true;
    }
    
    return false;
}

bool BarService::assignMaterial(const QUuid &barId, const QUuid &materialId)
{
    auto barOpt = m_repository->findBar(barId);
    if (!barOpt.has_value()) {
        return false;
    }
    
    Bar bar = barOpt.value();
    bar.setMaterialId(materialId);
    
    if (m_repository->updateBar(bar)) {
        emit barPropertiesAssigned(barId);
        emit barUpdated(barId);
        return true;
    }
    
    return false;
}

bool BarService::assignSection(const QUuid &barId, const QUuid &sectionId)
{
    auto barOpt = m_repository->findBar(barId);
    if (!barOpt.has_value()) {
        return false;
    }
    
    Bar bar = barOpt.value();
    bar.setSectionId(sectionId);
    
    if (m_repository->updateBar(bar)) {
        emit barPropertiesAssigned(barId);
        emit barUpdated(barId);
        return true;
    }
    
    return false;
}

bool BarService::assignProperties(const QUuid &barId, const QUuid &materialId, const QUuid &sectionId)
{
    auto barOpt = m_repository->findBar(barId);
    if (!barOpt.has_value()) {
        return false;
    }
    
    Bar bar = barOpt.value();
    bar.setMaterialId(materialId);
    bar.setSectionId(sectionId);
    
    if (m_repository->updateBar(bar)) {
        emit barPropertiesAssigned(barId);
        emit barUpdated(barId);
        return true;
    }
    
    return false;
}

int BarService::assignPropertiesToMultipleBars(const std::vector<QUuid> &barIds,
                                               const QUuid &materialId,
                                               const QUuid &sectionId)
{
    int count = 0;
    for (const auto &barId : barIds) {
        if (assignProperties(barId, materialId, sectionId)) {
            ++count;
        }
    }
    return count;
}

bool BarService::setKPoint(const QUuid &barId, const Vector3 &kPoint)
{
    auto barOpt = m_repository->findBar(barId);
    if (!barOpt.has_value()) {
        return false;
    }
    
    Bar bar = barOpt.value();
    bar.setKPoint(kPoint);
    
    if (m_repository->updateBar(bar)) {
        emit barUpdated(barId);
        return true;
    }
    
    return false;
}

bool BarService::clearKPoint(const QUuid &barId)
{
    auto barOpt = m_repository->findBar(barId);
    if (!barOpt.has_value()) {
        return false;
    }
    
    Bar bar = barOpt.value();
    bar.clearKPoint();
    
    if (m_repository->updateBar(bar)) {
        emit barUpdated(barId);
        return true;
    }
    
    return false;
}

double BarService::calculateBarLength(const QUuid &barId) const
{
    auto barOpt = m_repository->findBar(barId);
    if (!barOpt.has_value()) {
        return 0.0;
    }
    
    const Bar &bar = barOpt.value();
    
    auto startNodeOpt = m_repository->findNode(bar.startNodeId());
    auto endNodeOpt = m_repository->findNode(bar.endNodeId());
    
    if (!startNodeOpt.has_value() || !endNodeOpt.has_value()) {
        return 0.0;
    }
    
    return Bar::calculateLength(startNodeOpt->position(), endNodeOpt->position());
}

std::optional<Bar> BarService::findBar(const QUuid &id) const
{
    return m_repository->findBar(id);
}

std::optional<Bar> BarService::findBarByExternalId(int externalId) const
{
    return m_repository->findBarByExternalId(externalId);
}

std::vector<Bar> BarService::allBars() const
{
    return m_repository->allBars();
}

size_t BarService::barCount() const
{
    return m_repository->barCount();
}

std::vector<Bar> BarService::findBarsConnectedToNode(const QUuid &nodeId) const
{
    return m_repository->findBarsConnectedToNode(nodeId);
}

int BarService::nextExternalId() const
{
    auto bars = m_repository->allBars();
    if (bars.empty()) {
        return 1;
    }
    
    int maxId = 0;
    for (const auto &bar : bars) {
        if (bar.externalId() > maxId) {
            maxId = bar.externalId();
        }
    }
    
    return maxId + 1;
}

bool BarService::barExists(const QUuid &id) const
{
    return m_repository->findBar(id).has_value();
}

bool BarService::validateBarConnectivity(const QUuid &barId) const
{
    auto barOpt = m_repository->findBar(barId);
    if (!barOpt.has_value()) {
        return false;
    }
    
    const Bar &bar = barOpt.value();
    return m_repository->findNode(bar.startNodeId()).has_value() &&
           m_repository->findNode(bar.endNodeId()).has_value();
}

} // namespace Structura::App
