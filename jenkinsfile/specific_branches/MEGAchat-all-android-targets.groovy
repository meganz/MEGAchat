def failedTargets = []
def sendAndroidSlackComment(String reportPath, String fallbackWhenMissing, String logsUrl) {
    def mrURL = "${env.GIT_URL_ANDROID_MRS}/${env.gitlabMergeRequestIid}"
    def body = fallbackWhenMissing
    def logs = logsUrl ? "\n🪵 <${logsUrl}|Build Logs>" : ""
    if (fileExists(reportPath)) {
        body = readFile(reportPath).trim()
    } else {
        body = "${fallbackWhenMissing}\n⚠️ File not found: ${reportPath}"
        echo "⚠️ Slack report file not found: ${reportPath}"
    }

    body += "\n\n🔗 Triggered from: <${mrURL}|Merge Request>${logs}"

    withCredentials([string(credentialsId: 'slack_webhook_sdk_android_AAR_report', variable: 'SLACK_WEBHOOK_URL')]) {
        sh """
            curl -sS -X POST -H 'Content-type: application/json' --data "{\\"text\\": \\"${body}\\"}" \${SLACK_WEBHOOK_URL} || true
        """
    }
}
def sendAndroidGitlabComment(String reportPath, String fallbackWhenMissing, String logsUrl) {
    def logs = logsUrl ? "<br/>🪵 <a href='${logsUrl}'>Build Logs</a>" : ""
    if (fileExists(reportPath)) {
        def body = readFile(reportPath).trim() + logs
        addGitLabMRComment comment: body
    } else {
        def note = "<br/>⚠️ File `${reportPath}` not found in workspace."
        addGitLabMRComment comment: "${fallbackWhenMissing}${note}${logs}"
        echo "File ${reportPath} not found."
    }
    echo "✅ Comment sent to MR."
}

// Uploads a file to artifactory
String uploadToArtifactory(String fileName) {
    def targetPath
    withCredentials([
        usernamePassword(credentialsId: 'ANDROID_ARTIFACTORY_TOKEN', usernameVariable: 'ARTIFACTORY_USER', passwordVariable: 'TOKEN')
    ])  {
        def timestamp = sh(
            script: "date +'%Y%m%d_%H%M%S'",
            returnStdout: true
        ).trim()
        def logName = "prebuilt-sdk-log-${timestamp}.log"
        targetPath = "android-mega/cicd/sdk-android-logs/${logName}"
        sh """
            jf rt upload \
                "${fileName}" \
                --url ${REPO_URL} \
                --access-token ${TOKEN} \
                "${targetPath}"
        """
    }
    def link = "${REPO_URL}/${targetPath}"
    echo "Logs uploaded to: ${link}"
    return link
}

// Downloads the console log from this Jenkins build
void downloadJenkinsConsoleLog(String fileName) {
    withCredentials([usernameColonPassword(credentialsId: 'jenkins-ro', variable: 'CREDENTIALS')]) {
        sh "curl -u \"\${CREDENTIALS}\" ${BUILD_URL}consoleText -o ${fileName}"
    }
}

// Downloads the logs of the build, uploads them to artifactory
// And return the URL
String getLogsUrl(String projectId) {
    String message = ""
    String fileName = "build.log"
    String logUrl = ""
    downloadJenkinsConsoleLog(fileName)
    return uploadToArtifactory(fileName)
}

pipeline {
    agent { label 'linux && amd64 && docker && android' }
    options {
        timeout(time: 300, unit: 'MINUTES')
        buildDiscarder(logRotator(numToKeepStr: '135', daysToKeepStr: '21'))
        gitLabConnection('GitLabConnectionJenkins')
    }
    parameters {
        booleanParam(name: 'NIGHTLY_RESULTS_TO_SLACK', defaultValue: true, description: 'Should the job result be sent to slack? (ignored when BUILD_AARS=true)')
        booleanParam(name: 'BUILD_AARS', defaultValue: false, description: 'Build & publish AARs with sdk-packer')        
        booleanParam(name: 'BUILD_ARM', defaultValue: true, description: 'Build for ARM')
        booleanParam(name: 'BUILD_ARM64', defaultValue: true, description: 'Build for ARM64')
        booleanParam(name: 'BUILD_X86', defaultValue: true, description: 'Build for X86')
        booleanParam(name: 'BUILD_X64', defaultValue: true, description: 'Build for X64')
        string(name: 'MEGACHAT_BRANCH', defaultValue: 'develop', description: 'Define a custom SDK branch.')
        string(name: 'SDK_BRANCH', defaultValue: 'develop', description: 'Define a custom SDK branch.')
        string(name: 'BUILD_TYPE', defaultValue: 'dev', description: 'sdk-packer -Pbuild-type (dev|rel)')
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
        stage('Override build parameters'){
        when {
            expression { return env.gitlabTriggerPhrase?.trim() }
        }
            steps {
                script{
                    def SDK_BRANCH_FROM_TRIGGER = sh(script: 'echo "$gitlabTriggerPhrase" | grep --only-matching "\\-\\-sdk-branch=[^ ]*" | awk -F "sdk-branch="  \'{print \$2}\'|| :', returnStdout: true).trim()
                    def MEGACHAT_BRANCH_FROM_TRIGGER = sh(script: 'echo "$gitlabTriggerPhrase" | grep --only-matching "\\-\\-chat-branch=[^ ]*" | awk -F "chat-branch="  \'{print \$2}\'|| :', returnStdout: true).trim()
                    def BUILD_TYPE_FROM_TRIGGER = sh(script: 'echo "$gitlabTriggerPhrase" | grep --only-matching "\\-\\-lib-type=[^ ]*" | awk -F "lib-type="  \'{print \$2}\'|| :', returnStdout: true).trim()
                    env.SDK_COMMIT      = sh(script: 'echo "$gitlabTriggerPhrase" | grep --only-matching "\\-\\-sdk-commit=[^ ]*" | awk -F "sdk-commit="  \'{print \$2}\'|| :', returnStdout: true).trim()
                    env.MEGACHAT_COMMIT = sh(script: 'echo "$gitlabTriggerPhrase" | grep --only-matching "\\-\\-chat-commit=[^ ]*" | awk -F "chat-commit="  \'{print \$2}\'|| :', returnStdout: true).trim()
                    env.SDK_BRANCH      = SDK_BRANCH_FROM_TRIGGER  ?: params.SDK_BRANCH
                    env.MEGACHAT_BRANCH = MEGACHAT_BRANCH_FROM_TRIGGER ?: params.MEGACHAT_BRANCH
                    env.BUILD_TYPE      = BUILD_TYPE_FROM_TRIGGER ?: params.BUILD_TYPE
                    env.BUILD_AARS      = "true"
                    env.NIGHTLY_RESULTS_TO_SLACK = "false"
                    echo "SDK_BRANCH=${env.SDK_BRANCH}"
                    echo "MEGACHAT_BRANCH=${env.MEGACHAT_BRANCH}"
                    echo "SDK_COMMIT=${env.SDK_COMMIT}"
                    echo "MEGACHAT_COMMIT=${env.MEGACHAT_COMMIT}"                    
                    echo "BUILD_TYPE=${env.BUILD_TYPE}"
                    echo "BUILD_AARS=${env.BUILD_AARS}"
                }
            }
        }
        stage('Checkout SDK and MEGAchat'){
            steps {
                deleteDir()
                sh "echo Cloning MEGAchat branch \"${env.MEGACHAT_BRANCH}\""
                checkout([
                    $class: 'GitSCM',
                    branches: [[name: "${env.MEGACHAT_BRANCH}"]],
                    userRemoteConfigs: [[ url: "git@code.developers.mega.co.nz:megachat/MEGAchat.git", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                    extensions: [
                        [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                    ]
                ])
                script {
                    if (env.MEGACHAT_COMMIT?.trim()) {
                        sh "echo checking out to provided MEGAchat commit"
                        sh "git checkout ${env.MEGACHAT_COMMIT}"
                    }
                }
                dir('third-party/mega'){
                    sh "echo Cloning SDK branch \"${env.SDK_BRANCH}\""
                    checkout([
                        $class: 'GitSCM',
                        branches: [[name: "${env.SDK_BRANCH}"]],
                        userRemoteConfigs: [[ url: "git@code.developers.mega.co.nz:sdk/sdk.git", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                        extensions: [
                            [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                        ]
                    ])
                    script {
                        if (env.SDK_COMMIT?.trim()) {
                            sh "echo checking out to provided SDK commit"
                            sh "git checkout ${env.SDK_COMMIT}"
                        }
                    }
                }
                script{
                    megachat_sources_workspace = WORKSPACE
                    sdk_sources_workspace = "${megachat_sources_workspace}/third-party/mega"
                }
            }
        }

        stage('Define build trigger') {
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
        stage('Prepare sdk-packer input, checkout and run it') {
            when { expression { return env.BUILD_AARS == 'true' } }
            environment {
                ANDROID_HOME = "/home/jenkins/android-cmdlinetools/"
                ANDROID_NDK_HOME ="/home/jenkins/android-ndk/"

            }
            steps {
                script {
                    def archs = []
                    if (params.BUILD_ARM)   archs << "arm"
                    if (params.BUILD_ARM64) archs << "arm64"
                    if (params.BUILD_X86)   archs << "x86"
                    if (params.BUILD_X64)   archs << "x64"  

                    packerRootDir = "${WORKSPACE}/output/sdk_packer/"
                    archs.each { arch ->
                        def outputDir = "${WORKSPACE}/output/android-dynamic/${arch}"
                        def packerArchDir = "${packerRootDir}/${arch}"
                        sh """
                            mkdir -p ${packerArchDir}/bindings
                            echo "=== Listing ${outputDir} ==="
                            ls -lR ${outputDir} || true
                            cp ${outputDir}/bindings/java/libmega.so ${packerArchDir}/
                            cp `find ${outputDir} -name "libwebrtc.jar"` ${packerRootDir}/
                            cp -r ${outputDir}/bindings/java/nz ${packerArchDir}/bindings/
                            cp -r ${outputDir}/bindings/java/nz ${packerArchDir}/bindings/
                        """
                    }
                }
                dir("${workspace}/packer") {
                    checkout([
                        $class: 'GitSCM',
                        branches: [[name: "main"]],
                        userRemoteConfigs: [[ url: "git@code.developers.mega.co.nz:mobile/android/ci-cd/sdk-packer.git", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                        extensions: [
                            [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                        ]
                    ])
                    script{
                        withCredentials([
                            usernamePassword(credentialsId: 'ANDROID_ARTIFACTORY_TOKEN', usernameVariable: 'ARTIFACTORY_USER', passwordVariable: 'ARTIFACTORY_ACCESS_TOKEN')
                        ]) {
                            withEnv(["ARTIFACTORY_BASE_URL=${env.REPOSITORY_URL}"]) {
                                sh """
                                    ls -lR ${packerRootDir}
                                    chmod +x ./gradlew
                                    ./gradlew clean
                                    ./gradlew sdk-packer:artifactoryPublish \\
                                    -Psdk-root='${sdk_sources_workspace}' \\
                                    -Pchat-root='${megachat_sources_workspace}' \\
                                    -Poutput-root='${packerRootDir}' \\
                                    -Pbuild-type='${BUILD_TYPE}'
                                """   
                            }
                        }                        
                    }
                    sh "ls -l sdk-packer/build/outputs/aar || true"
                }
            }
        }
    }
    post {
        always {
            sh "docker image rm meganz/megachat-android-build-env:${env.BUILD_NUMBER}"
            script {
                if (env.NIGHTLY_RESULTS_TO_SLACK == "true") {
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
        }
        success {
            script{
                if (env.gitlabTriggerPhrase?.trim()) {
                    def logsUrl = getLogsUrl(env.PROJECT_ID)
                    sendAndroidGitlabComment(
                        "${workspace}/packer/gitlab_report.txt",
                        "Build succeeded<br/>Build results: [Jenkins [${env.BUILD_DISPLAY_NAME}]](${env.RUN_DISPLAY_URL})",
                        logsUrl
                    )
                    sendAndroidSlackComment(
                        "${workspace}/packer/slack_report.txt",
                        "✅ Android SDK Build SUCCESS\\nBuild: ${env.BUILD_DISPLAY_NAME}\\nURL: ${env.RUN_DISPLAY_URL}",
                        logsUrl
                    )  
                }
            }
            deleteDir() /* clean up our workspace */
        }
        failure {
            script{
                if (env.gitlabTriggerPhrase?.trim()) {
                    def logsUrl = getLogsUrl(env.PROJECT_ID)
                    sendAndroidGitlabComment(
                        "${workspace}/packer/gitlab_report.txt",
                        ":red_circle: ${env.JOB_NAME} :penguin: <b>Android</b> FAILURE :worried:<br/>Build results: [Jenkins [${env.BUILD_DISPLAY_NAME}]](${env.RUN_DISPLAY_URL})<br/>",
                        logsUrl
                    )
                    sendAndroidSlackComment(
                        "${workspace}/packer/slack_report.txt",
                        "🔴 Android SDK Build FAILURE\\nBuild: ${env.BUILD_DISPLAY_NAME}\\nURL: ${env.RUN_DISPLAY_URL}",
                        logsUrl
                    ) 
                }
            }
            deleteDir() /* clean up our workspace */
        }
        aborted {
            script{
                if (env.gitlabTriggerPhrase?.trim()) {
                    def logsUrl = getLogsUrl(env.PROJECT_ID)
                    sendAndroidGitlabComment(
                        "${workspace}/packer/gitlab_report.txt",
                        ":interrobang: :penguin: <b>Android SDK Build</b> ABORTED :confused:<br/>Build results: [Jenkins [${env.BUILD_DISPLAY_NAME}]](${env.RUN_DISPLAY_URL})<br/>",
                        logsUrl
                    )
                    sendAndroidSlackComment(
                        "${workspace}/packer/slack_report.txt",
                        "⚠️  Android SDK Build ABORTED\\nBuild: ${env.BUILD_DISPLAY_NAME}\\nURL: ${env.RUN_DISPLAY_URL}",
                        logsUrl
                    ) 
                }
            }
            deleteDir() /* clean up our workspace */
        }
    }
}
// vim: syntax=groovy tabstop=4 shiftwidth=4
