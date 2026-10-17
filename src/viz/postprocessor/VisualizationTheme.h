#pragma once

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Structura::Visualization {

/// Constantes visuais centralizadas do sistema de pós-processamento.
/// Alterar aqui afeta toda a visualização de forma consistente.
struct VisualizationTheme {

    // ── Cores dos diagramas ────────────────────────────────────────────────
    /// RGB(45, 110, 255) normalizado — região positiva
    static constexpr double posColorR = 45.0  / 255.0;
    static constexpr double posColorG = 110.0 / 255.0;
    static constexpr double posColorB = 255.0 / 255.0;

    /// RGB(220, 50, 50) normalizado — região negativa
    static constexpr double negColorR = 220.0 / 255.0;
    static constexpr double negColorG = 50.0  / 255.0;
    static constexpr double negColorB = 50.0  / 255.0;

    static constexpr double diagramOpacity  = 0.65; ///< Transparência do preenchimento
    static constexpr double outlineWidth    = 2.5;  ///< Espessura do contorno em px
    static constexpr double outlineOpacity  = 0.95; ///< Opacidade do contorno
    static constexpr double hatchOpacity    = 0.40; ///< Opacidade da hachura

    // ── Iluminação — visual técnico, sem brilho excessivo ─────────────────
    static constexpr double ambient  = 0.40;
    static constexpr double diffuse  = 0.60;
    static constexpr double specular = 0.05;

    // ── Amostragem ─────────────────────────────────────────────────────────
    static constexpr int    minSamples      = 24;   ///< Mínimo de amostras por elemento
    static constexpr int    maxSamples      = 120;  ///< Máximo de amostras por elemento
    static constexpr double samplesPerMeter = 6.0;  ///< amostras por metro (esforços são polinômios de grau <= 2)

    // ── Torção ─────────────────────────────────────────────────────────────
    /// Ângulo máximo de torção da faixa helicoidal em graus.
    /// Normalizado por elemento — não depende do máximo global do modelo.
    static constexpr double maxTwistAngleDeg = 90.0;

    // ── Escala física padrão ───────────────────────────────────────────────
    /// Amplitude máxima = fração do comprimento médio das barras.
    /// userScale=1.0 → amplitude máx = 30% do comprimento médio.
    static constexpr double defaultAmplitudeFraction = 0.30;

    /// Largura da faixa helicoidal de torção (fração da amplitude máxima).
    static constexpr double torsionBandWidthFraction = 0.15;

    // ── Glifos ─────────────────────────────────────────────────────────────
    static constexpr double supportGlyphSize    = 0.25; ///< Fração do comprimento médio
    static constexpr double arrowGlyphSize      = 0.20;
    static constexpr int    distributedArrSpacing = 3;  ///< setas por metro de barra
};

} // namespace Structura::Visualization
