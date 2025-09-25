def failedTargets = []

pipeline {
    agent { label 'linux && amd64 && docker' }
    options {
        timeout(time: 300, unit: 'MINUTES')
        buildDiscarder(logRotator(numToKeepStr: '135', daysToKeepStr: '21'))
        gitLabConnection('GitLabConnectionJenkins')
    }
    parameters {
        booleanParam(name: 'RESULT_TO_SLACK', defaultValue: true, description: 'Should the job result be sent to slack?')
        booleanParam(name: 'UPLOAD_TO_ARTIFACTORY', defaultValue: false, description: 'Upload debug symbols tarball to Artifactory?')        
        booleanParam(name: 'BUILD_ARM', defaultValue: true, description: 'Build for ARM')
        booleanParam(name: 'BUILD_ARM64', defaultValue: true, description: 'Build for ARM64')
        booleanParam(name: 'BUILD_X86', defaultValue: true, description: 'Build for X86')
        booleanParam(name: 'BUILD_X64', defaultValue: true, description: 'Build for X64')
        string(name: 'MEGACHAT_BRANCH', defaultValue: 'develop', description: 'Define a custom SDK branch.')
        string(name: 'SDK_BRANCH', defaultValue: 'develop', description: 'Define a custom SDK branch.')
    }
    environment {
        VCPKGPATH = "/opt/vcpkg"
        // VCPKGPATH_CACHE = "${HOME}/.cache/vcpkg"
        VCPKG_BINARY_SOURCES = 'clear;x-aws,s3://vcpkg-cache/archives/,readwrite'
        AWS_ACCESS_KEY_ID = credentials('s4_access_key_id_vcpkg_cache')
        AWS_SECRET_ACCESS_KEY = credentials('s4_secret_access_key_vcpkg_cache')
        AWS_ENDPOINT_URL = "https://s3.g.s4.mega.io"
    }
    stages {
        stage('Checkout SDK and MEGAchat'){
            steps {
                deleteDir()
                sh "echo Cloning MEGAchat branch \"${params.MEGACHAT_BRANCH}\""
                checkout([
                    $class: 'GitSCM',
                    branches: [[name: "${params.MEGACHAT_BRANCH}"]],
                    userRemoteConfigs: [[ url: "git@code.developers.mega.co.nz:megachat/MEGAchat.git", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                    extensions: [
                        [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                    ]
                ])
                dir('third-party/mega'){
                    sh "echo Cloning SDK branch \"${params.SDK_BRANCH}\""
                    checkout([
                        $class: 'GitSCM',
                        branches: [[name: "${params.SDK_BRANCH}"]],
                        userRemoteConfigs: [[ url: "git@code.developers.mega.co.nz:sdk/sdk.git", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                        extensions: [
                            [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                        ]
                    ])
                }
                script{
                    megachat_sources_workspace = WORKSPACE
                    sdk_sources_workspace = "${megachat_sources_workspace}/third-party/mega"
                }
            }
        }

        stage('Override static build if needed') {
            steps {
                script {
                    def cause = currentBuild.getBuildCauses().toString()
                    if (cause.contains("Started by timer")) {
                        env.BUILD_TRIGGERED_BY_TIMER = 'true'
                    } else {
                        env.BUILD_TRIGGERED_BY_TIMER = 'false'
                    }
                }
            }
        }

        stage('Build Android docker image'){
            steps{
                dir("dockerfile"){
                    sh "docker build -t meganz/megachat-android-build-env:${env.BUILD_NUMBER} -f ./android-cross-build.dockerfile ."
                }
                sh "mkdir -p ${WORKSPACE}/output/android-dynamic/arm64"
                sh "mkdir -p ${WORKSPACE}/output/android-dynamic/arm"
                sh "mkdir -p ${WORKSPACE}/output/android-dynamic/x86"
                sh "mkdir -p ${WORKSPACE}/output/android-dynamic/x64"      
            }
        }
        stage('Get UID and GID') {
            steps {
                script {
                    def uid = sh(script: 'id -u', returnStdout: true).trim()
                    def gid = sh(script: 'id -g', returnStdout: true).trim()
                    env.UID = uid
                    env.GID = gid
                }
            }
        }
        stage('Build with docker'){
            parallel {
                stage('Build arm'){
                    when {
                        beforeAgent true
                        expression { params.BUILD_ARM == true }
                    }
                    steps {
                        sh """
                            docker run \
                                --name megachat-android-builder-arm-dynamiclib-${env.BUILD_NUMBER} \
                                --rm \
                                -v ${WORKSPACE}:/mega/MEGAchat \
                                -v ${VCPKGPATH}:/mega/vcpkg \
                                -v ${WORKSPACE}/output/android-dynamic/arm:/mega/build-MEGAchat-mega-android \
                                -e VCPKG_BINARY_SOURCES \
                                -e AWS_ACCESS_KEY_ID \
                                -e AWS_SECRET_ACCESS_KEY \
                                -e AWS_ENDPOINT_URL \
                                -e ARCH=arm \
                                meganz/megachat-android-build-env:${env.BUILD_NUMBER}
                        """
                    }
                    post{
                        aborted {
                            sh """
                                docker kill megachat-android-builder-arm-${env.BUILD_NUMBER}
                                docker kill megachat-android-builder-arm-dynamiclib-${env.BUILD_NUMBER}
                            """
                            script {
                                failedTargets.add("arm")
                            }
                        }
                        failure {
                            script {
                                failedTargets.add("arm")
                            }
                        }
                    }
                }
                stage('Build arm64'){
                    when {
                        beforeAgent true
                        expression { params.BUILD_ARM64 == true }
                    }
                    steps {
                        sh """
                            docker run \
                                --name megachat-android-builder-arm64-dynamiclib-${env.BUILD_NUMBER} \
                                --rm -v ${WORKSPACE}:/mega/MEGAchat \
                                -v ${VCPKGPATH}:/mega/vcpkg \
                                -v ${WORKSPACE}/output/android-dynamic/arm64:/mega/build-MEGAchat-mega-android \
                                -e ARCH=arm64 \
                                -e VCPKG_BINARY_SOURCES \
                                -e AWS_ACCESS_KEY_ID \
                                -e AWS_SECRET_ACCESS_KEY \
                                -e AWS_ENDPOINT_URL \
                                meganz/megachat-android-build-env:${env.BUILD_NUMBER}
                        """
                    }
                    post{
                        aborted {
                            sh """
                                docker kill megachat-android-builder-arm64-${env.BUILD_NUMBER}
                                docker kill megachat-android-builder-arm64-dynamiclib-${env.BUILD_NUMBER}
                            """
                            script {
                                failedTargets.add("arm64")
                            }
                        }
                        failure {
                            script {
                                failedTargets.add("arm64")
                            }
                        }
                    }
                }
                stage('Build x86'){
                    when {
                        beforeAgent true
                        expression { params.BUILD_X86 == true }
                    }
                    steps {
                        sh """
                            docker run \
                                --name megachat-android-builder-x86-dynamiclib-${env.BUILD_NUMBER} \
                                --rm \
                                -v ${WORKSPACE}:/mega/MEGAchat \
                                -v ${VCPKGPATH}:/mega/vcpkg \
                                -v ${WORKSPACE}/output/android-dynamic/x86:/mega/build-MEGAchat-mega-android \
                                -e ARCH=x86 \
                                -e VCPKG_BINARY_SOURCES \
                                -e AWS_ACCESS_KEY_ID \
                                -e AWS_SECRET_ACCESS_KEY \
                                -e AWS_ENDPOINT_URL \
                                meganz/megachat-android-build-env:${env.BUILD_NUMBER}
                        """
                    }
                    post{
                        aborted {
                            sh """
                                docker kill megachat-android-builder-x86-${env.BUILD_NUMBER}
                                docker kill megachat-android-builder-x86-dynamiclib-${env.BUILD_NUMBER}
                            """
                            script {
                                failedTargets.add("x86")
                            }
                        }
                        failure {
                            script {
                                failedTargets.add("x86")
                            }
                        }
                    }
                }
                stage('Build x64'){
                    when {
                        beforeAgent true
                        expression { params.BUILD_X64 == true }
                    }
                    steps {
                        sh """
                            docker run \
                                --name megachat-android-builder-x64-dynamiclib-${env.BUILD_NUMBER} \
                                --rm \
                                -v ${WORKSPACE}:/mega/MEGAchat \
                                -v ${VCPKGPATH}:/mega/vcpkg \
                                -v ${WORKSPACE}/output/android-dynamic/x64:/mega/build-MEGAchat-mega-android \
                                -e ARCH=x64 \
                                -e VCPKG_BINARY_SOURCES \
                                -e AWS_ACCESS_KEY_ID \
                                -e AWS_SECRET_ACCESS_KEY \
                                -e AWS_ENDPOINT_URL \
                                meganz/megachat-android-build-env:${env.BUILD_NUMBER}
                        """
                    }
                    post{
                        aborted {
                            sh """
                                docker kill megachat-android-builder-x64-${env.BUILD_NUMBER}
                                docker kill megachat-android-builder-x64-dynamiclib-${env.BUILD_NUMBER}
                            """
                            script {
                                failedTargets.add("x64")
                            }
                        }
                        failure {
                            script {
                                failedTargets.add("x64")
                            }
                        }
                    }
                }
            }
        }
        stage('Post-processing: cleanup, .so collection, and upload to artifactory') {
            when {
                expression {
                    return env.BUILD_TRIGGERED_BY_TIMER == 'false' && params.UPLOAD_TO_ARTIFACTORY 
                }
            }
            steps{
                script {
                    def archs = []
                    if (params.BUILD_ARM)   archs << "arm"
                    if (params.BUILD_ARM64) archs << "arm64"
                    if (params.BUILD_X86)   archs << "x86"
                    if (params.BUILD_X64)   archs << "x64"

                    // 1. Collect .so with symbols and bindings .java
                    archs.each { arch ->
                        def outputDir = "${WORKSPACE}/output/android-dynamic/${arch}"
                        def archTargetDir = "${WORKSPACE}/debug_symbols/${arch}"
                        sh """
                            mkdir -p ${archTargetDir}/bindings
                            echo "=== Listing ${outputDir} ==="
                            ls -lR ${outputDir} || true
                            cp ${outputDir}/bindings/java/libmega.so ${archTargetDir}/
                            cp -r ${outputDir}/bindings/java/nz ${archTargetDir}/bindings/
                        """
                    }
                    // 2. Tarball with symbols
                    def tarball = "debug_symbols_${env.BUILD_NUMBER}.tar.gz"
                    sh "tar czf ${WORKSPACE}/${tarball} -C ${WORKSPACE} debug_symbols"
                    
                    // 3. Upload to artifactory
                    withCredentials([string(credentialsId: 'MEGACHAT_ARTIFACTORY_TOKEN', variable: 'MEGACHAT_ARTIFACTORY_TOKEN')]) {
                        sh """
                            jf rt upload \
                                ${tarball} \
                                --url ${REPO_URL} \
                                --access-token ${MEGACHAT_ARTIFACTORY_TOKEN} \
                                MEGAchat/android-build/
                        """
                    }
                    echo "Packages successfully uploaded. URL: [${env.REPO_URL}/MEGAchat/android-build/${tarball}]"
                }
            }
        }
    }
    post {
        always {
            sh "docker image rm meganz/megachat-android-build-env:${env.BUILD_NUMBER}"
            script {
                if (params.RESULT_TO_SLACK) {
                    def sdk_commit = sh(script: "git -C ${sdk_sources_workspace} rev-parse HEAD", returnStdout: true).trim()
                    def megachat_commit = sh(script: "git -C ${megachat_sources_workspace} rev-parse HEAD", returnStdout: true).trim()
                    def messageStatus = currentBuild.currentResult
                    def messageColor = messageStatus == 'SUCCESS'? "#00FF00": "#FF0000" //green or red
                    def message = """
                        *MEGAchat Android* <${BUILD_URL}|Build result>: '${messageStatus}'.
                        MEGAchat branch: `${MEGACHAT_BRANCH}`
                        MEGAchat commit: `${megachat_commit}`
                        SDK branch: `${SDK_BRANCH}`
                        SDK commit: `${sdk_commit}`
                    """.stripIndent()
                    
                    if (failedTargets.size() > 0) {
                        message += "\nFailed targets: ${failedTargets.join(', ')}"
                    }

                    withCredentials([string(credentialsId: 'slack_webhook_sdk_report', variable: 'SLACK_WEBHOOK_URL')]) {
                        sh """
                            curl -X POST -H 'Content-type: application/json' --data '
                                {
                                "attachments": [
                                    {
                                        "color": "${messageColor}",
                                        "blocks": [
                                        {
                                            "type": "section",
                                            "text": {
                                                    "type": "mrkdwn",
                                                    "text": "${message}"
                                            }
                                        }
                                        ]
                                    }
                                    ]
                                }' \${SLACK_WEBHOOK_URL}
                        """
                    }
                }
            }
            deleteDir() /* clean up our workspace */
        }
    }
}
// vim: syntax=groovy tabstop=4 shiftwidth=4
