package dev.codexbeer.dashboard

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val LightBeer = lightColorScheme(
    primary = Color(0xFF805000), onPrimary = Color.White,
    primaryContainer = Color(0xFFFFDEA5), onPrimaryContainer = Color(0xFF291800),
    secondary = Color(0xFF6D593D), onSecondary = Color.White,
    secondaryContainer = Color(0xFFF0DFC5), onSecondaryContainer = Color(0xFF291D0B),
    surfaceContainerHigh = Color(0xFFF4EDE2), surfaceTint = Color(0xFF805000),
    background = Color(0xFFFFFBF5), onBackground = Color(0xFF24211C),
    surface = Color(0xFFFFFBF5), onSurface = Color(0xFF24211C),
    surfaceVariant = Color(0xFFF0E7D9), onSurfaceVariant = Color(0xFF51473A),
    outline = Color(0xFF807565), error = Color(0xFFBA1A1A)
)
private val DarkBeer = darkColorScheme(
    primary = Color(0xFFF4BD61), onPrimary = Color(0xFF432C00),
    primaryContainer = Color(0xFF614000), onPrimaryContainer = Color(0xFFFFDEA5),
    secondary = Color(0xFFDBC3A0), onSecondary = Color(0xFF3D2E18),
    secondaryContainer = Color(0xFF4B3D29), onSecondaryContainer = Color(0xFFF0DFC5),
    surfaceContainerHigh = Color(0xFF302C25), surfaceTint = Color(0xFFF4BD61),
    background = Color(0xFF181715), onBackground = Color(0xFFECE5DB),
    surface = Color(0xFF181715), onSurface = Color(0xFFECE5DB),
    surfaceVariant = Color(0xFF302C25), onSurfaceVariant = Color(0xFFD4C7B5),
    outline = Color(0xFF9D907D), error = Color(0xFFFFB4AB)
)

@Composable
fun BeerTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = if (isSystemInDarkTheme()) DarkBeer else LightBeer, content = content)
}
