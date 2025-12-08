#!/bin/bash

# Konfiguracja
REPO_DIR="/c/Projekty/github/WinDefCtl"
TAG="v1.0.0"
REPO="wesmar/WinDefCtl"

cd "$REPO_DIR" || exit 1

echo "======================================"
echo "🔧 KROK 1: Pakowanie plików"
echo "======================================"
./pack-data.sh
if [ $? -ne 0 ]; then
    echo "❌ Błąd pakowania!"
    exit 1
fi

echo ""
echo "======================================"
echo "🗑️  KROK 2: Usuwanie starych assetów"
echo "======================================"

# Usuń stare WinDefCtl.7z
gh release delete-asset "$TAG" WinDefCtl.7z --yes 2>/dev/null && echo "✅ Usunięto WinDefCtl.7z" || echo "⚠️  WinDefCtl.7z nie istniało"

echo ""
echo "======================================"
echo "📤 KROK 3: Upload nowych plików"
echo "======================================"

gh release upload "$TAG" \
    "data/WinDefCtl.7z#WinDefCtl.7z" \
    --clobber

if [ $? -eq 0 ]; then
    echo ""
    echo "======================================"
    echo "✅ SUKCES!"
    echo "======================================"
    echo "Release zaktualizowany: https://github.com/$REPO/releases/tag/$TAG"
else
    echo "❌ Błąd uploadu!"
    exit 1
fi