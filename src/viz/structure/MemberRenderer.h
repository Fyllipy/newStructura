#pragma once

#include <QUuid>
#include <QSet>

#include <vtkSmartPointer.h>
#include <vtkNew.h>

class vtkRenderer;
class vtkActor;
class vtkPolyData;
class vtkPoints;
class vtkCellArray;
class vtkUnsignedCharArray;

namespace Structura::Visualization {

class MemberRenderer
{
public:
    MemberRenderer();
    ~MemberRenderer();

    void addToRenderer(vtkRenderer* renderer);
    void removeFromRenderer(vtkRenderer* renderer);

    void setPoints(vtkPoints* points);

    void clear();
    void addBar(const QUuid& id, vtkIdType nodeI_idx, vtkIdType nodeJ_idx);

    void setHoveredBar(const QUuid& id);
    void clearHoveredBar();
    void setSelectedBars(const QSet<QUuid>& ids);

    void setVisible(bool visible);
    bool isVisible() const;

    vtkActor* getActor() const { return m_actor.GetPointer(); }

private:
    void rebuildColors();

    vtkNew<vtkPolyData> m_barData;
    vtkNew<vtkCellArray> m_barLines;
    vtkNew<vtkUnsignedCharArray> m_colors;
    vtkNew<vtkActor> m_actor;

    std::vector<QUuid> m_bars;
    
    QUuid m_hoveredBar;
    QSet<QUuid> m_selectedBars;

    // Design tokens (Cores modernas para barras)
    static constexpr unsigned char COLOR_NORMAL[3] = {148, 163, 184};   // Slate 400
    static constexpr unsigned char COLOR_HOVER[3] = {56, 189, 248};     // Sky 400
    static constexpr unsigned char COLOR_SELECTED[3] = {245, 158, 11};  // Amber 500
};

} // namespace Structura::Visualization
