#pragma once

#include <QObject>
#include <QUuid>
#include <QHash>
#include <QSet>
#include <QVector>
#include <QVector3D>
#include <QMatrix4x4>
#include <QMap>

#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vector>
#include <memory>
#include <optional>
#include <array>

#include "viz/AreaPanelRenderer.h"
#include "viz/ModeShapeRenderer.h"
#include "viz/BarTypeRenderer.h"
#include "core/model/Node.h"
#include "core/model/Bar.h"
#include "core/model/Material.h"
#include "core/model/Section.h"
#include "app/GeometrySnapshot.h"
#include "app/ISceneEditGateway.h"
#include "app/ISceneModelGateway.h"
#include "viz/postprocessor/SupportGlyphFactory.h"
#include "viz/structure/StructureRenderer.h"
#include "viz/structure/ConnectionRenderer.h"
#include "viz/structure/ElementAppearance.h"
namespace Structura::Construction {
class ConstructionScene;
}
namespace Structura::Visualization {
class LoadActionRenderer;
class SolidMemberRenderer;
}

class QVTKOpenGLNativeWidget;
class vtkGenericOpenGLRenderWindow;
class vtkRenderer;
class vtkRenderWindow;
class vtkAxesActor;
class vtkOrientationMarkerWidget;
class vtkRenderWindowInteractor;
class vtkPoints;
class vtkPolyData;
class vtkCellArray;
class vtkPolyDataMapper;
class vtkActor;
class vtkCellPicker;
class vtkUnsignedCharArray;
class vtkPointPicker;
class vtkGlyph3D;
class vtkArrowSource;
class vtkDoubleArray;
class vtkBillboardTextActor3D;

/// Rotulos de cargas: posicoes no mundo e textos formatados.
struct LoadLabelData {
    std::vector<std::array<double, 3>> positions;
    std::vector<QString> texts;
    std::vector<std::array<double, 3>> colors;   ///< cor (0..1) do desenho da carga
    std::vector<double> weights;                 ///< |valor| (prioridade do rotulo)
};

class SceneController : public QObject,
                        public Structura::App::ISceneEditGateway,
                        public Structura::App::ISceneModelGateway
{
    Q_OBJECT

public:
    static constexpr double kCoordTolerance = 1e-6;

    using Node = Structura::Model::Node;
    using Bar = Structura::Model::Bar;

    struct NodalLoadVisual {
        QVector3D position;
        QVector3D force;
        QVector3D moment;
    };

    /// Uma carga uniforme (forca ou momento por comprimento) em uma barra.
    struct MemberActionVisual {
        QVector3D start;
        QVector3D end;
        QVector3D xAxis;  ///< Eixos locais da barra (mundo)
        QVector3D yAxis;
        QVector3D zAxis;
        QVector3D values; ///< Componentes por eixo (kN/m ou kNm/m)
        bool moment {false};
        bool local {false};
        bool selfWeight {false};
    };

    struct SupportVisual {
        QVector3D position;
        std::array<bool, 6> restraints; // UX, UY, UZ, RX, RY, RZ
    };

    using NodeInfo = Structura::App::SceneNodeInfo;
    using BarInfo = Structura::App::SceneBarInfo;

    explicit SceneController(QObject *parent = nullptr);
    ~SceneController() override;

    void initialize(QVTKOpenGLNativeWidget *vtkWidget);

    // Plano de construcao (grid de referencia, eixos e snaps de lancamento)
    [[nodiscard]] Structura::Construction::ConstructionScene *constructionScene() const { return m_construction.get(); }
    [[nodiscard]] vtkRenderer *renderer() const;
    [[nodiscard]] vtkRenderWindow *renderWindow() const;
    /// Redesenha a cena (publico para controladores de lancamento).
    void render() { requestRender(); }

    // Visibilidade de elementos (barra de opcoes de exibicao)
    void setNodesVisible(bool visible);
    void setSupportsVisible(bool visible);
    void setConstraintsVisible(bool visible);
    [[nodiscard]] bool nodesVisible() const { return m_showNodes; }
    [[nodiscard]] bool supportsVisible() const { return m_showSupports; }

signals:
    /// Emitido (potencialmente muitas vezes) quando a camera muda.
    void cameraChanged();
    /// Emitido apos nos/barras mudarem e a geometria de construcao ser recalculada.
    void geometryRefreshed();
    /// Emitido quando as cargas sao redesenhadas (rotulos mudaram).
    void loadLabelsChanged();

public:
    /// Matriz composta (projecao * visao) da camera atual, para projetar pontos do mundo.
    [[nodiscard]] QMatrix4x4 compositeMatrix() const;

public slots:
    QUuid addPoint(double x, double y, double z);
    QUuid addPointWithId(double x, double y, double z, int externalId);
    void clearAll();
    void resetCamera();
    void zoomExtents();
    /// Centraliza a camera nas barras (mantem a direcao de visao), com margem em torno delas.
    void focusOnBars(const QSet<QUuid> &barIds);

public:
    // Picking API (screen to world)
    bool pickWorldPoint(int displayX, int displayY, double &x, double &y, double &z) const;
    int viewportHeight() const;
    bool worldPointOnPlaneZ0(int displayX, int displayY, double &x, double &y, double &z) const;
    bool worldPointOnViewPlane(int displayX, int displayY, double &x, double &y, double &z) const;


    // Loads
    void setNodalLoadVisuals(const QVector<NodalLoadVisual> &visuals);
    void setMemberActionVisuals(const QVector<MemberActionVisual> &visuals);
    void setLoadVisibility(bool nodeForces, bool nodeMoments, bool memberForces, bool memberMoments);

    /// Panos de cargas de area (superficie, setas de pressao e acao nas barras).
    void setAreaPanels(const std::vector<Structura::Visualization::AreaPanelRenderer::Panel> &panels);
    void setAreaPanelsVisible(bool visible);
    /// Forma modal / deformada desenhada sobre o modelo (lista vazia remove).
    /// Marcadores de rotulas, trelicas e tirantes (lista vazia remove).
    void setBarTypeMarkers(const std::vector<Structura::Visualization::BarTypeRenderer::Marker> &markers);
    /// Camera atual (posicao, alvo, cima em m; abertura vertical em graus; paralela?). false sem janela de visualizacao.
    bool cameraState(double position[3], double target[3], double up[3], double &viewAngleDeg, bool &parallel) const;
    void setModeShape(const std::vector<std::vector<std::array<double, 3>>> &lines);

    /// Barra com secao extrudada (exibicao solida).
    struct SolidMemberVisual {
        QUuid barId;   ///< barra de origem (para destacar o solido ao selecionar)
        QVector3D start;
        QVector3D end;
        QVector3D xAxis;
        QVector3D yAxis;
        QVector3D zAxis;
        QString family;
        QMap<QString, double> parameters;
        bool mirrored {false};
        bool hasColor {false};
        unsigned char color[3] {180, 189, 199};
        bool dimmed {false};
    };
    void setSolidMembers(const QVector<SolidMemberVisual> &members);

    /// Esmaece/oculta/colore nos e barras (isolar nivel, ocultar, colorir por secao/material).
    void setElementAppearance(const Structura::Visualization::AppearanceMap &nodes,
                              const Structura::Visualization::AppearanceMap &bars);
    /// Camera perpendicular ao plano cuja normal e o eixo `normalAxis` (0=X, 1=Y, 2=Z), centrada no nivel.
    void lookAtPlane(int normalAxis, double level);
    /// Projecao da camera: paralela (isometrica/ortografica, padrao) ou em perspectiva. Mantem o enquadramento atual.
    void setPerspective(bool perspective);
    [[nodiscard]] bool isPerspective() const;
    /// Camera olhando o modelo a partir da direcao (dx,dy,dz) do olho em relacao ao centro, com o vetor "para cima" dado.
    void lookFromDirection(double dx, double dy, double dz, double ux, double uy, double uz);
    /// Restaura uma camera salva (posicao, alvo, cima, abertura em graus; paralela = projecao ortografica).
    void setCameraState(const double position[3], const double target[3], const double up[3], double viewAngleDeg, bool parallel);
    void setSolidsVisible(bool visible);
    /// Sólidos esquemáticos das ligações (chapas, parafusos, soldas) já em coordenadas do modelo.
    void setConnectionMesh(const Structura::Visualization::ConnectionRenderer::Mesh &mesh);
    void setConnectionsVisible(bool visible);
    /// Marca um no (esfera translucida) para o usuario identificar de qual no se trata; sem posicao, remove a marca.
    void setNodeHighlight(const std::optional<QVector3D> &position);
    /// Rotulos (valores) das cargas para a camada de texto da viewport.
    [[nodiscard]] LoadLabelData loadLabels() const;

    // Supports (restraints)
    void setSupportVisuals(const QVector<SupportVisual> &visuals);
    
    // Bar Local Coordinate Systems (LCS)
    void setShowBarLCS(bool show);
    bool isShowingBarLCS() const { return m_showBarLCS; }
    void updateBarLCSVisuals();
    /// Giro da secao (graus) por ID externo de barra; usado nos eixos locais exibidos.
    void setBarSectionRotations(const QHash<int, double> &rotationsByExternalId);

    // Nodes
    int nodeCount() const;
    std::vector<NodeInfo> nodeInfos() const;
    const Node *findNode(const QUuid &id) const;
    Node *findNode(const QUuid &id);
    QUuid pickNode(int displayX, int displayY) const;
    QUuid pickBar(int displayX, int displayY) const;
    void setHighlightedNode(const QUuid &nodeId);
    void clearHighlightedNode();
    void setSelectedNodes(const QSet<QUuid> &nodeIds);
    bool updateNodePosition(const QUuid &nodeId, double x, double y, double z);
    bool updateNodePositions(const QVector<QUuid> &nodeIds, const QVector<QVector3D> &positions);

    /// Copia da topologia (ids internos e externos preservados) para excluir/dividir/desfazer.
    using GeometrySnapshot = Structura::App::GeometrySnapshot;
    [[nodiscard]] GeometrySnapshot captureGeometry() const;
    /// Substitui toda a topologia (nos e barras) pela copia, mantendo os mesmos ids.
    void restoreGeometry(const GeometrySnapshot &geometry);

    // Bars
    QUuid addBar(const QUuid &startNodeId,
                 const QUuid &endNodeId,
                 const QUuid &materialId,
                 const QUuid &sectionId);
    void assignBarProperties(const std::vector<QUuid> &barIds,
                             const std::optional<QUuid> &materialId,
                             const std::optional<QUuid> &sectionId);
    void setBarExternalId(const QUuid &barId, int externalId);
    std::vector<BarInfo> bars() const;
    const Bar *findBar(const QUuid &id) const;
    Bar *findBar(const QUuid &id);
    void setSelectedBars(const QSet<QUuid> &barIds);
    void applySolidSelection();

private:
    void updateBounds();
    int nodeIndex(const QUuid &id) const;
    int barIndex(const QUuid &id) const;
    void initializeBarLcsRendering();
    void initializePickers();
    void clearNodes();
    void clearBars();
    void clearLoads();
    void clearSupports();
    void requestRender();

    void updateLoadVisuals();
    void updateSupportVisuals();
    void scheduleConstructionRefresh();
    void refreshConstructionGeometry();
    void rebuildBarLCSVisuals();

    vtkNew<vtkGenericOpenGLRenderWindow> m_renderWindow;
    vtkNew<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkOrientationMarkerWidget> m_orientationMarker;

    std::unique_ptr<Structura::Visualization::StructureRenderer> m_structureRenderer;
    std::unique_ptr<Structura::Construction::ConstructionScene> m_construction;
    bool m_constructionRefreshPending {false};
    bool m_renderPending {false};
    bool m_showNodes {true};
    bool m_showSupports {true};




    // Load visualization (Vector Glyphs)
    QVector<NodalLoadVisual> m_nodalLoadVisuals;
    QVector<MemberActionVisual> m_memberActionVisuals;
    std::unique_ptr<Structura::Visualization::LoadActionRenderer> m_loadRenderer;
    std::unique_ptr<Structura::Visualization::AreaPanelRenderer> m_areaPanelRenderer;
    std::unique_ptr<Structura::Visualization::ModeShapeRenderer> m_modeShapeRenderer;
    std::unique_ptr<Structura::Visualization::BarTypeRenderer> m_barTypeRenderer;
    std::vector<Structura::Visualization::AreaPanelRenderer::Panel> m_areaPanels;
    bool m_areaPanelsVisible {true};
    std::unique_ptr<Structura::Visualization::SolidMemberRenderer> m_solidRenderer;
    std::unique_ptr<Structura::Visualization::ConnectionRenderer> m_connectionRenderer;
    vtkSmartPointer<vtkActor> m_nodeHighlight;
    struct LoadVisibility {
        bool nodeForces {true};
        bool nodeMoments {true};
        bool memberForces {true};
        bool memberMoments {true};
    } m_loadVisibility;
    
    
    

    // Support visualization
    QVector<SupportVisual> m_supportVisuals;
    vtkSmartPointer<vtkPolyData> m_supportData;
    vtkSmartPointer<vtkPolyDataMapper> m_supportMapper;
    vtkSmartPointer<vtkActor> m_supportActor;
    vtkSmartPointer<vtkPolyDataMapper> m_constraintMapper;
    vtkSmartPointer<vtkActor> m_constraintActor;
    bool m_showConstraints {true};
    
    // Bar LCS visualization
    bool m_showBarLCS {false};
    QHash<int, double> m_barSectionRotations;
    vtkSmartPointer<vtkPolyData> m_lcsData;
    vtkSmartPointer<vtkPoints> m_lcsPoints;
    vtkSmartPointer<vtkCellArray> m_lcsCells;
    vtkSmartPointer<vtkUnsignedCharArray> m_lcsColors;
    vtkSmartPointer<vtkPolyDataMapper> m_lcsMapper;
    vtkSmartPointer<vtkActor> m_lcsActor;

    // Picker
    vtkSmartPointer<vtkCellPicker> m_picker;
    vtkSmartPointer<vtkPointPicker> m_nodePicker;
    vtkSmartPointer<vtkCellPicker> m_barPicker;

    std::vector<Node> m_nodes;
    QHash<QUuid, int> m_nodeIndexById;

    std::vector<Bar> m_bars;
    QHash<QUuid, int> m_barIndexById;

    QUuid m_highlightNodeId;
    QSet<QUuid> m_selectedNodeIds;
    QSet<QUuid> m_selectedBarIds;
    QVector<QUuid> m_solidBarIds;   ///< barra de cada solido, na ordem de setSolidMembers
    QSet<QUuid> m_solidSelection;
    

    int m_nextNodeExternalId {1};
};
