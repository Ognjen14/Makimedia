import QtQuick
import "../Singletons"

Text {
    color: AppTheme.textDisabled
    font.pixelSize: AppTheme.fs12
    font.weight: Font.Medium
    font.letterSpacing: 0.06 * AppTheme.fs12
    font.capitalization: Font.AllUppercase
    topPadding: AppTheme.spacing20
    bottomPadding: AppTheme.spacing10
}
