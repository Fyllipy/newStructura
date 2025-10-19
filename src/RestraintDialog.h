#pragma once

#include <QDialog>
#include <array>

class QCheckBox;
class QLabel;

/**
 * @brief Dialog for setting nodal restraints (boundary conditions)
 * 
 * Allows the user to specify which degrees of freedom (DOFs) are fixed:
 * - Translations: UX, UY, UZ
 * - Rotations: RX, RY, RZ
 */
class RestraintDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RestraintDialog(QWidget *parent = nullptr);
    ~RestraintDialog() override = default;

    /**
     * @brief Get the restraint state for all 6 DOFs
     * @return Array of 6 booleans: [UX, UY, UZ, RX, RY, RZ]
     */
    std::array<bool, 6> restraints() const;

    /**
     * @brief Set the initial restraint state
     * @param restraints Array of 6 booleans: [UX, UY, UZ, RX, RY, RZ]
     */
    void setRestraints(const std::array<bool, 6> &restraints);

    /**
     * @brief Set whether to show a "mixed values" state
     * 
     * When multiple nodes with different restraints are selected,
     * this can be used to indicate that values vary.
     * 
     * @param mixed If true, checkboxes will be shown in a partial/tri-state
     */
    void setMixedState(bool mixed);

private:
    void setupUI();
    void createDOFGroup(const QString &title, int startIndex);

    std::array<QCheckBox *, 6> m_restraintChecks;
    QLabel *m_infoLabel;
    bool m_mixedState {false};
};
