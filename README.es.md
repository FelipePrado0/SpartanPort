# infamous_firstlight

Investigación experimental de un port nativo para PC de **inFAMOUS First Light** (PS4),
objetivo **CUSA00575**, Windows 11 y Vulkan.

Este repositorio parte de `gow3_pc`, reutilizando su arquitectura de ejecución nativa x86-64 y
su núcleo gráfico derivado de shadPS4. **Todavía no es jugable**: el trabajo actual está centrado
en loader, imports, runtime y primer arranque controlado.

No se incluyen archivos del juego. Utiliza un dump de tu propia copia.

Consulta [README.md](README.md) para el estado técnico actual y
[docs/BUILD_WINDOWS.md](docs/BUILD_WINDOWS.md) para compilar.
